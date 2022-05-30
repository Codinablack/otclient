/*
 * Copyright (c) 2010-2020 OTClient <https://github.com/edubart/otclient>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "soundeffect.h"
#include "soundmanager.h"
#include "soundsource.h"
#include "declarations.h"
#include "soundbuffer.h"
#include "soundfile.h"

#include "framework/stdext/time.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx.h>
#include <AL/efx-presets.h>

/* Effect object functions */
static LPALGENEFFECTS alGenEffects;
static LPALDELETEEFFECTS alDeleteEffects;
static LPALISEFFECT alIsEffect;
static LPALEFFECTI alEffecti;
static LPALEFFECTIV alEffectiv;
static LPALEFFECTF alEffectf;
static LPALEFFECTFV alEffectfv;
static LPALGETEFFECTI alGetEffecti;
static LPALGETEFFECTIV alGetEffectiv;
static LPALGETEFFECTF alGetEffectf;
static LPALGETEFFECTFV alGetEffectfv;

/* Auxiliary Effect Slot object functions */
static LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots;
static LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots;
static LPALISAUXILIARYEFFECTSLOT alIsAuxiliaryEffectSlot;
static LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti;
static LPALAUXILIARYEFFECTSLOTIV alAuxiliaryEffectSlotiv;
static LPALAUXILIARYEFFECTSLOTF alAuxiliaryEffectSlotf;
static LPALAUXILIARYEFFECTSLOTFV alAuxiliaryEffectSlotfv;
static LPALGETAUXILIARYEFFECTSLOTI alGetAuxiliaryEffectSloti;
static LPALGETAUXILIARYEFFECTSLOTIV alGetAuxiliaryEffectSlotiv;
static LPALGETAUXILIARYEFFECTSLOTF alGetAuxiliaryEffectSlotf;
static LPALGETAUXILIARYEFFECTSLOTFV alGetAuxiliaryEffectSlotfv;

SoundEffect::SoundEffect(ALCdevice* device)
{
    m_device = device;
    m_effectId = 0;
    m_effectSlot = 0;

    m_presets = {
        {"generic", EFX_REVERB_PRESET_GENERIC},
        {"paddedCell", EFX_REVERB_PRESET_PADDEDCELL},
        {"room", EFX_REVERB_PRESET_ROOM},
        {"sewerPipe", EFX_REVERB_PRESET_SEWERPIPE},
        {"underWater", EFX_REVERB_PRESET_UNDERWATER}
    };

#define LOAD_PROC(T, x)  ((x) = (T)alGetProcAddress(#x))
    LOAD_PROC(LPALGENEFFECTS, alGenEffects);
    LOAD_PROC(LPALDELETEEFFECTS, alDeleteEffects);
    LOAD_PROC(LPALISEFFECT, alIsEffect);
    LOAD_PROC(LPALEFFECTI, alEffecti);
    LOAD_PROC(LPALEFFECTIV, alEffectiv);
    LOAD_PROC(LPALEFFECTF, alEffectf);
    LOAD_PROC(LPALEFFECTFV, alEffectfv);
    LOAD_PROC(LPALGETEFFECTI, alGetEffecti);
    LOAD_PROC(LPALGETEFFECTIV, alGetEffectiv);
    LOAD_PROC(LPALGETEFFECTF, alGetEffectf);
    LOAD_PROC(LPALGETEFFECTFV, alGetEffectfv);

    LOAD_PROC(LPALGENAUXILIARYEFFECTSLOTS, alGenAuxiliaryEffectSlots);
    LOAD_PROC(LPALDELETEAUXILIARYEFFECTSLOTS, alDeleteAuxiliaryEffectSlots);
    LOAD_PROC(LPALISAUXILIARYEFFECTSLOT, alIsAuxiliaryEffectSlot);
    LOAD_PROC(LPALAUXILIARYEFFECTSLOTI, alAuxiliaryEffectSloti);
    LOAD_PROC(LPALAUXILIARYEFFECTSLOTIV, alAuxiliaryEffectSlotiv);
    LOAD_PROC(LPALAUXILIARYEFFECTSLOTF, alAuxiliaryEffectSlotf);
    LOAD_PROC(LPALAUXILIARYEFFECTSLOTFV, alAuxiliaryEffectSlotfv);
    LOAD_PROC(LPALGETAUXILIARYEFFECTSLOTI, alGetAuxiliaryEffectSloti);
    LOAD_PROC(LPALGETAUXILIARYEFFECTSLOTIV, alGetAuxiliaryEffectSlotiv);
    LOAD_PROC(LPALGETAUXILIARYEFFECTSLOTF, alGetAuxiliaryEffectSlotf);
    LOAD_PROC(LPALGETAUXILIARYEFFECTSLOTFV, alGetAuxiliaryEffectSlotfv);
#undef LOAD_PROC


    /* Query for Effect Extension */
    if (alcIsExtensionPresent(m_device, "ALC_EXT_EFX") == AL_FALSE) {
        g_logger.error(stdext::format("unable to locate OpenAl EFX extension"));
    } else {
        if (!alGenEffects) {
            g_logger.error(stdext::format("unable to load OpenAl EFX extension"));
            return;
        } else {
            //g_logger.info(stdext::format("loaded OpenAl EFX extension, it sould work now"));
            alGenEffects(1, &m_effectId);
            alGenAuxiliaryEffectSlots(1, &m_effectSlot);

            alAuxiliaryEffectSloti(m_effectSlot, AL_EFFECTSLOT_EFFECT, (ALint)m_effectId);
            assert(alGetError()==AL_NO_ERROR && "Failed to set effect slot");
        }
    }
    assert(alGetError() == AL_NO_ERROR);
}

SoundEffect::~SoundEffect()
{
    if (m_effectId != 0) {
        alDeleteEffects(1, &m_effectId);
        alDeleteAuxiliaryEffectSlots(1, &m_effectSlot);
        assert(alGetError() == AL_NO_ERROR);
    }
}

void SoundEffect::loadPreset(EFXEAXREVERBPROPERTIES* preset)
{
    if(alGetEnumValue("AL_EFFECT_EAXREVERB") != 0) {
        //printf("Using EAX Reverb\n");

        /* EAX Reverb is available. Set the EAX effect type then load the
        * reverb properties. */
        alEffecti(m_effectId, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);

        alEffectf(m_effectId, AL_EAXREVERB_DENSITY, preset->flDensity);
        alEffectf(m_effectId, AL_EAXREVERB_DIFFUSION, preset->flDiffusion);
        alEffectf(m_effectId, AL_EAXREVERB_GAIN, preset->flGain);
        alEffectf(m_effectId, AL_EAXREVERB_GAINHF, preset->flGainHF);
        alEffectf(m_effectId, AL_EAXREVERB_GAINLF, preset->flGainLF);
        alEffectf(m_effectId, AL_EAXREVERB_DECAY_TIME, preset->flDecayTime);
        alEffectf(m_effectId, AL_EAXREVERB_DECAY_HFRATIO, preset->flDecayHFRatio);
        alEffectf(m_effectId, AL_EAXREVERB_DECAY_LFRATIO, preset->flDecayLFRatio);
        alEffectf(m_effectId, AL_EAXREVERB_REFLECTIONS_GAIN, preset->flReflectionsGain);
        alEffectf(m_effectId, AL_EAXREVERB_REFLECTIONS_DELAY, preset->flReflectionsDelay);
        alEffectfv(m_effectId, AL_EAXREVERB_REFLECTIONS_PAN, preset->flReflectionsPan);
        alEffectf(m_effectId, AL_EAXREVERB_LATE_REVERB_GAIN, preset->flLateReverbGain);
        alEffectf(m_effectId, AL_EAXREVERB_LATE_REVERB_DELAY, preset->flLateReverbDelay);
        alEffectfv(m_effectId, AL_EAXREVERB_LATE_REVERB_PAN, preset->flLateReverbPan);
        alEffectf(m_effectId, AL_EAXREVERB_ECHO_TIME, preset->flEchoTime);
        alEffectf(m_effectId, AL_EAXREVERB_ECHO_DEPTH, preset->flEchoDepth);
        alEffectf(m_effectId, AL_EAXREVERB_MODULATION_TIME, preset->flModulationTime);
        alEffectf(m_effectId, AL_EAXREVERB_MODULATION_DEPTH, preset->flModulationDepth);
        alEffectf(m_effectId, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, preset->flAirAbsorptionGainHF);
        alEffectf(m_effectId, AL_EAXREVERB_HFREFERENCE, preset->flHFReference);
        alEffectf(m_effectId, AL_EAXREVERB_LFREFERENCE, preset->flLFReference);
        alEffectf(m_effectId, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, preset->flRoomRolloffFactor);
        alEffecti(m_effectId, AL_EAXREVERB_DECAY_HFLIMIT, preset->iDecayHFLimit);
    } else {
        //printf("Using Standard Reverb\n");

        /* No EAX Reverb. Set the standard reverb effect type then load the
         * available reverb properties. */
        alEffecti(m_effectId, AL_EFFECT_TYPE, AL_EFFECT_REVERB);

        alEffectf(m_effectId, AL_REVERB_DENSITY, preset->flDensity);
        alEffectf(m_effectId, AL_REVERB_DIFFUSION, preset->flDiffusion);
        alEffectf(m_effectId, AL_REVERB_GAIN, preset->flGain);
        alEffectf(m_effectId, AL_REVERB_GAINHF, preset->flGainHF);
        alEffectf(m_effectId, AL_REVERB_DECAY_TIME, preset->flDecayTime);
        alEffectf(m_effectId, AL_REVERB_DECAY_HFRATIO, preset->flDecayHFRatio);
        alEffectf(m_effectId, AL_REVERB_REFLECTIONS_GAIN, preset->flReflectionsGain);
        alEffectf(m_effectId, AL_REVERB_REFLECTIONS_DELAY, preset->flReflectionsDelay);
        alEffectf(m_effectId, AL_REVERB_LATE_REVERB_GAIN, preset->flLateReverbGain);
        alEffectf(m_effectId, AL_REVERB_LATE_REVERB_DELAY, preset->flLateReverbDelay);
        alEffectf(m_effectId, AL_REVERB_AIR_ABSORPTION_GAINHF, preset->flAirAbsorptionGainHF);
        alEffectf(m_effectId, AL_REVERB_ROOM_ROLLOFF_FACTOR, preset->flRoomRolloffFactor);
        alEffecti(m_effectId, AL_REVERB_DECAY_HFLIMIT, preset->iDecayHFLimit);
    }

    //alAuxiliaryEffectSloti(m_effectSlot, AL_EFFECTSLOT_EFFECT, (ALint)m_effectId);
    //assert(alGetError()==AL_NO_ERROR && "Failed to set effect slot");
    //if (alIsEffect(m_effectId))
        //std::cout << "Successfully created effect object\n";

    /* Update effect slot */
    alAuxiliaryEffectSloti(m_effectSlot, AL_EFFECTSLOT_EFFECT, (ALint)m_effectId);
    assert(alGetError()==AL_NO_ERROR && "Failed to set effect slot");
}

void SoundEffect::setPreset(std::string presetName)
{
    auto search = m_presets.find(presetName);
    if (search != m_presets.end()) {
        //std::cout << "Found " << search->first << '\n';
        loadPreset(&search->second);
    } else {
        g_logger.error(std::format("Could not find preset matching: %s\n", presetName));
    }
}
