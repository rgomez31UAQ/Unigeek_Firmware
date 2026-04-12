//
// LilyGO T-Embed CC1101 speaker — NS4168 I2S amp, no codec init required
//

#pragma once
#include "core/SpeakerI2S.h"

// NS4168 is a direct I2S amp — SpeakerI2S handles everything via SPK_BCLK/WCLK/DOUT pins
using SpeakerEmbedCC1101 = SpeakerI2S;
