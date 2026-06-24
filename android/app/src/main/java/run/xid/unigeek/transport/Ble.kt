package run.xid.unigeek.transport

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.ParcelUuid
import kotlinx.coroutines.CancellableContinuation
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.callbackFlow
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeout
import run.xid.unigeek.protocol.Proto
import java.io.IOException
import java.util.UUID
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException
import kotlin.math.min

private val CCCD_UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
private val NUS_SERVICE_UUID = UUID.fromString(Proto.NUS_SERVICE)
private val NUS_RX_UUID = UUID.fromString(Proto.NUS_RX)
private val NUS_TX_UUID = UUID.fromString(Proto.NUS_TX)

data class BleDevice(val device: BluetoothDevice, val name: String?, val rssi: Int)

object BleSupport {
    fun isEnabled(ctx: Context): Boolean {
        val mgr = ctx.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager
        return mgr?.adapter?.isEnabled == true
    }

    /** Emit UniGeek devices (advertising the NUS service) as they're discovered. */
    @SuppressLint("MissingPermission")
    fun scan(ctx: Context): Flow<BleDevice> = callbackFlow {
        val mgr = ctx.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val scanner = mgr.adapter?.bluetoothLeScanner
            ?: run { close(IllegalStateException("Turn on Bluetooth to scan.")); return@callbackFlow }
        val filter = ScanFilter.Builder()
            .setServiceUuid(ParcelUuid.fromString(Proto.NUS_SERVICE))
            .build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        val cb = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                trySend(
                    BleDevice(
                        result.device,
                        result.scanRecord?.deviceName ?: runCatching { result.device.name }.getOrNull(),
                        result.rssi,
                    )
                )
            }
            override fun onScanFailed(errorCode: Int) {
                close(IllegalStateException("BLE scan failed (code $errorCode)"))
            }
        }
        scanner.startScan(listOf(filter), settings, cb)
        awaitClose { runCatching { scanner.stopScan(cb) } }
    }
}

/**
 * BLE transport over Nordic UART Service. Connects, negotiates a large MTU, enables
 * TX notifications, and writes RX in ATT-sized slices (the firmware's stream parser
 * stitches them back into frames). Writes are serialized and flow-controlled on the
 * per-write completion callback.
 */
@SuppressLint("MissingPermission")
class BleTransport(
    private val context: Context,
    private val device: BluetoothDevice,
    private val onBytes: (ByteArray) -> Unit,
    private val onClosed: (Throwable?) -> Unit,
) : Transport {

    override val chunkSize = Proto.PUT_CHUNK_BLE
    override val label = "BLE · " + (runCatching { device.name }.getOrNull() ?: device.address)

    private var gatt: BluetoothGatt? = null
    private var rx: BluetoothGattCharacteristic? = null
    private val writeMutex = Mutex()
    private val writeAck = Channel<Int>(Channel.CONFLATED)
    private var ready = false
    private var connectCont: CancellableContinuation<Unit>? = null

    private fun finishConnect(error: Throwable?) {
        val c = connectCont ?: return
        connectCont = null
        if (error == null) c.resume(Unit) else c.resumeWithException(error)
    }

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    if (status != BluetoothGatt.GATT_SUCCESS) {
                        finishConnect(IOException("GATT connect failed (status $status)")); return
                    }
                    // Short connection interval — Android's default is slow and makes the
                    // mirror/transfers laggy. (Best effort; the peripheral may clamp it.)
                    runCatching { g.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH) }
                    if (!g.requestMtu(247)) g.discoverServices()
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    if (!ready) finishConnect(IOException("Disconnected before ready"))
                    else onClosed(IOException("BLE disconnected"))
                }
            }
        }

        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
            // Prefer the 2 Mbps PHY — roughly doubles throughput for the screen mirror
            // on phones + ESP32-S3 that support it. No-op / ignored otherwise.
            runCatching {
                g.setPreferredPhy(
                    BluetoothDevice.PHY_LE_2M_MASK,
                    BluetoothDevice.PHY_LE_2M_MASK,
                    BluetoothDevice.PHY_OPTION_NO_PREFERRED,
                )
            }
            g.discoverServices()
        }

        @Suppress("DEPRECATION")
        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                finishConnect(IOException("Service discovery failed (status $status)")); return
            }
            val svc = g.getService(NUS_SERVICE_UUID)
                ?: return finishConnect(IOException("Nordic UART service not found — is this a UniGeek?"))
            rx = svc.getCharacteristic(NUS_RX_UUID)
            val tx = svc.getCharacteristic(NUS_TX_UUID)
            if (rx == null || tx == null) {
                return finishConnect(IOException("UART characteristics missing"))
            }
            g.setCharacteristicNotification(tx, true)
            val cccd = tx.getDescriptor(CCCD_UUID)
                ?: return finishConnect(IOException("Notify descriptor missing"))
            cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
            g.writeDescriptor(cccd)
        }

        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) {
            if (d.uuid == CCCD_UUID) {
                if (status == BluetoothGatt.GATT_SUCCESS) { ready = true; finishConnect(null) }
                else finishConnect(IOException("Enabling notifications failed (status $status)"))
            }
        }

        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
            if (c.uuid == NUS_TX_UUID) c.value?.let(onBytes)
        }

        override fun onCharacteristicWrite(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) {
            writeAck.trySend(status)
        }
    }

    suspend fun connect() = withTimeout(20_000) {
        suspendCancellableCoroutine { cont ->
            connectCont = cont
            cont.invokeOnCancellation { runCatching { gatt?.disconnect(); gatt?.close() } }
            gatt = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
        }
    }

    @Suppress("DEPRECATION")
    override suspend fun write(data: ByteArray) {
        val r = rx ?: throw IOException("BLE not connected")
        val g = gatt ?: throw IOException("BLE not connected")
        writeMutex.withLock {
            var off = 0
            while (off < data.size) {
                val end = min(off + Proto.BLE_WRITE_MAX, data.size)
                r.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
                r.value = data.copyOfRange(off, end)
                if (!g.writeCharacteristic(r)) throw IOException("BLE write rejected")
                val status = withTimeout(4000) { writeAck.receive() }
                if (status != BluetoothGatt.GATT_SUCCESS) throw IOException("BLE write error (status $status)")
                off = end
            }
        }
    }

    override suspend fun close() {
        ready = false
        runCatching { gatt?.disconnect() }
        runCatching { gatt?.close() }
        gatt = null
        rx = null
    }
}
