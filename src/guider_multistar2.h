/*
 *  guider_multistar2.h
 *  PHD Guiding
 *
 *  Original guider_onestar Created by Craig Stark.
 *  Copyright (c) 2006-2010 Craig Stark.
 *  All rights reserved.
 *
 *  guider_onestar completely refactored by Bret McKee
 *  Copyright (c) 2012 Bret McKee
 *  All rights reserved.
 *
 *  guider_multistar extensions created by Bruce Waddington
 *  Copyright (c) 2020 Bruce Waddington
 *  All rights reserved.
 *
 *  guider_multistar2 created by Bryan Duke
 *  Copyright (c) 2026 Bryan Duke
 *  All rights reserved.
 *
 *  This source code is distributed under the following "BSD" license
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *    Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *    Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *    Neither the name of Bret McKee, Dad Dog Development,
 *     Craig Stark, Stark Labs nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */
 
#ifndef GUIDER_MULTISTAR2_H_INCLUDED
#define GUIDER_MULTISTAR2_H_INCLUDED
 
#include "guider_multistar.h"
#include <deque>
#include <utility>
#include <vector>
 
// Compile-time switch for extra multistar2 logging.
// Enable by adding -DMULTISTAR2_DEBUG_LOG=1 to your build.
#ifndef MULTISTAR2_DEBUG_LOG
#define MULTISTAR2_DEBUG_LOG 1
#endif

// Experimental multistar guider. Implements an aggregate "solution" star position
// derived from multiple guide stars, allowing guiding continuity as stars are lost/reacquired.
class GuiderMultiStar2 : public GuiderMultiStar
{
public:
    explicit GuiderMultiStar2(wxWindow *parent);
    ~GuiderMultiStar2() override;
 
    wxString GetSettingsSummary() const override;

    // Aggregate-solution behavior: guiding can continue even if the originally selected
    // primary star is lost, as long as at least one usable guide star remains.
    bool IsLocked() const override;
    const PHD_Point& CurrentPosition() const override;
    const Star& PrimaryStar() const override;
    wxString GetStarCount() const override;
    void OnPaint(wxPaintEvent& evt) override;

private:
    void InvalidateCurrentPosition(bool fullReset = false) override;
    bool UpdateCurrentPosition(const usImage *pImage, GuiderOffset *ofs, FrameDroppedInfo *errorInfo) override;

    struct StarState
    {
        PHD_Point lastPos;
        bool lastPosValid = false;
        bool foundThisFrame = false;
        bool contributingThisFrame = false;
        unsigned int reacquireGoodCount = 0;

        // Mass-change tracking (per star)
        std::deque<std::pair<wxLongLong_t, double>> massHist; // (time_ms, adj_mass)
        double highMass = 0.;
        double lowMass = 9e99;
    };

    void EnsureStarStateSize();

    Star m_solutionStar;   // estimated primary position (aggregate solution)
    Star m_displayStar;    // a real found star for UI/status (mass/SNR)
    unsigned int m_solutionStarsUsed = 0;        // contributing stars this frame
    unsigned int m_maxConcurrentStarsUsed = 0;   // max contributing since (re)select
    std::vector<StarState> m_starState;          // parallel to m_guideStars

#if MULTISTAR2_DEBUG_LOG
    // Debug-only state for change-triggered logging. Kept out of release builds.
    bool m_dbgInited = false;
    unsigned int m_dbgLastPoolSize = 0;
    unsigned int m_dbgLastFoundCount = 0;
    unsigned int m_dbgLastUsedCount = 0;
    bool m_dbgLastPrimaryContrib = false;
    PHD_Point m_dbgLastDisp;
    std::vector<bool> m_dbgLastContribMask; // per guide-star contributing flag
#endif
};
 
#endif /* GUIDER_MULTISTAR2_H_INCLUDED */

