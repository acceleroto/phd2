/*
 *  guider_multistar2.cpp
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
 
#include "phd.h"
#include "guider_multistar2.h"

#include <algorithm>
#include <deque>
#include <utility>
#include <vector>

// Compile-time switch for extra multistar2 logging:
// see `guider_multistar2.h` (or pass -DMULTISTAR2_DEBUG_LOG=1 in your build).

#if MULTISTAR2_DEBUG_LOG
#define MS2LOGF(...) Debug.Write(wxString::Format(__VA_ARGS__))
#else
#define MS2LOGF(...) \
    do              \
    {               \
    } while (0)
#endif
 
static wxString StarStatus2(const Star& star)
{
    wxString status = wxString::Format(_("m=%.0f SNR=%.1f"), star.Mass, star.SNR);

    if (star.GetError() == Star::STAR_SATURATED)
        status += _T(" ") + _("Saturated");

    int exp;
    bool auto_exp;
    pFrame->GetExposureInfo(&exp, &auto_exp);

    if (auto_exp)
    {
        status += _T(" ");
        if (exp >= 1)
            status += wxString::Format(_("Exp=%0.1f s"), (double) exp / 1000.);
        else
            status += wxString::Format(_("Exp=%d ms"), exp);
    }

    return status;
}

struct DistanceChecker2
{
    enum State
    {
        ST_GUIDING,
        ST_WAITING,
        ST_RECOVERING,
    };
    State m_state;
    wxLongLong_t m_expires;
    double m_forceTolerance;

    enum
    {
        WAIT_INTERVAL_MS = 5000
    };

    DistanceChecker2() : m_state(ST_GUIDING), m_forceTolerance(0.) { }

    static wxString StateName(State st)
    {
        switch (st)
        {
        case ST_GUIDING:
            return "guiding";
        case ST_WAITING:
            return "waiting";
        case ST_RECOVERING:
            return "recovering";
        default:
            return "unknown";
        }
    }

    State CurrentState() const { return m_state; }

    void Activate()
    {
        if (m_state == ST_GUIDING)
        {
            Debug.Write("DistanceChecker2: activated\n");
            m_state = ST_WAITING;
            m_expires = ::wxGetUTCTimeMillis().GetValue() + WAIT_INTERVAL_MS;
            m_forceTolerance = 2.0;
            MS2LOGF("MultiStar2: recovery state=%s event=activate tolerance=%.3f avgDist=nan threshold=nan dist=nan\n",
                    StateName(m_state), m_forceTolerance);
        }
    }

    static bool _CheckDistance(double distance, bool raOnly, double tolerance, double *avgDistOut, double *thresholdOut,
                               bool *statsValidOut)
    {
        enum
        {
            MIN_FRAMES_FOR_STATS = 10
        };
        Guider *guider = pFrame->pGuider;
        *avgDistOut = 0.;
        *thresholdOut = 0.;
        *statsValidOut = false;
        if (!guider->IsGuiding() || guider->IsPaused() || PhdController::IsSettling() ||
            guider->CurrentErrorFrameCount() < MIN_FRAMES_FOR_STATS)
        {
            return true;
        }
        double avgDist = guider->CurrentErrorSmoothed(raOnly);
        double threshold = tolerance * avgDist;
        *avgDistOut = avgDist;
        *thresholdOut = threshold;
        *statsValidOut = true;
        if (distance > threshold)
        {
            Debug.Write(wxString::Format("DistanceChecker2: reject for large offset (%.2f > %.2f) avgDist = %.2f count = %u\n",
                                         distance, threshold, avgDist, guider->CurrentErrorFrameCount()));
            return false;
        }
        return true;
    }

    bool CheckDistance(double distance, bool raOnly, double tolerance)
    {
        if (m_forceTolerance != 0.)
            tolerance = m_forceTolerance;

        double avgDist = 0.;
        double threshold = 0.;
        bool statsValid = false;
        bool small_offset = _CheckDistance(distance, raOnly, tolerance, &avgDist, &threshold, &statsValid);
        wxString avgDistStr = statsValid ? wxString::Format("%.3f", avgDist) : wxString("nan");
        wxString thresholdStr = statsValid ? wxString::Format("%.3f", threshold) : wxString("nan");

        switch (m_state)
        {
        default:
        case ST_GUIDING:
            if (small_offset)
                return true;

            Debug.Write("DistanceChecker2: activated\n");
            m_state = ST_WAITING;
            m_expires = ::wxGetUTCTimeMillis().GetValue() + WAIT_INTERVAL_MS;
            MS2LOGF("MultiStar2: recovery state=%s event=activate tolerance=%.3f avgDist=%s threshold=%s dist=%.3f\n",
                    StateName(m_state), tolerance, avgDistStr, thresholdStr, distance);
            return false;

        case ST_WAITING:
        {
            if (small_offset)
            {
                Debug.Write("DistanceChecker2: deactivated\n");
                m_state = ST_GUIDING;
                m_forceTolerance = 0.;
                MS2LOGF("MultiStar2: recovery state=%s event=deactivate tolerance=%.3f avgDist=%s threshold=%s dist=%.3f\n",
                        StateName(m_state), tolerance, avgDistStr, thresholdStr, distance);
                return true;
            }
            // large distance
            wxLongLong_t now = ::wxGetUTCTimeMillis().GetValue();
            if (now < m_expires)
            {
                // reject frame
                return false;
            }
            // timed-out
            Debug.Write("DistanceChecker2: begin recovering\n");
            m_state = ST_RECOVERING;
            MS2LOGF("MultiStar2: recovery state=%s event=timeout tolerance=%.3f avgDist=%s threshold=%s dist=%.3f\n",
                    StateName(m_state), tolerance, avgDistStr, thresholdStr, distance);
            // fall through
        }

        case ST_RECOVERING:
            if (small_offset)
            {
                Debug.Write("DistanceChecker2: deactivated\n");
                m_state = ST_GUIDING;
                MS2LOGF("MultiStar2: recovery state=%s event=deactivate tolerance=%.3f avgDist=%s threshold=%s dist=%.3f\n",
                        StateName(m_state), tolerance, avgDistStr, thresholdStr, distance);
            }
            return true;
        }
    }
};

static DistanceChecker2 s_distanceChecker2;

#if MULTISTAR2_JUMP_DIAGNOSTICS
// TODO(multistar2-jump-diagnostics): TEMPORARY
// Remove with the matching declarations and capture sites after the jump investigation.
void GuiderMultiStar2::JumpDiagReset()
{
    m_jumpDiagLastAcceptedValid = false;
    m_jumpDiagLastAcceptedDisp.SetXY(0.0, 0.0);
    m_jumpDiagPreFrames.clear();
    m_jumpDiagLargeCandidateTimes.clear();
    m_jumpDiagEpisodeId = 0;
    m_jumpDiagCollectingPost = false;
    m_jumpDiagPostRemaining = 0;
    m_jumpDiagEpisodeReason.clear();
    m_jumpDiagCooldownUntil = 0;
}

wxString GuiderMultiStar2::JumpDiagTriggerReason(const JumpDiagFrameSnapshot& snapshot) const
{
    if (!snapshot.guiding || snapshot.paused || snapshot.settling)
        return wxEmptyString;

    if (snapshot.jumpRejected)
        return "jump_reject";
    if (snapshot.referenceRepinned)
        return "reference_repin";

    const double threshold = snapshot.imageScaleKnown ? 2.0 / snapshot.imageScale : 1.0;
    const double returnTolerance = snapshot.imageScaleKnown ? 0.5 / snapshot.imageScale : 0.5;
    auto Distance = [](const PHD_Point& a, const PHD_Point& b) {
        const double dx = a.X - b.X;
        const double dy = a.Y - b.Y;
        return sqrt(dx * dx + dy * dy);
    };

    if (snapshot.candidateValid && m_jumpDiagPreFrames.size() >= 2)
    {
        const JumpDiagFrameSnapshot& b = m_jumpDiagPreFrames[m_jumpDiagPreFrames.size() - 1];
        const JumpDiagFrameSnapshot& a = m_jumpDiagPreFrames[m_jumpDiagPreFrames.size() - 2];
        if (a.guiding && !a.paused && !a.settling && b.guiding && !b.paused && !b.settling && a.candidateValid &&
            b.candidateValid && Distance(a.candidate, b.candidate) > threshold &&
            Distance(b.candidate, snapshot.candidate) > threshold &&
            Distance(a.candidate, snapshot.candidate) <= returnTolerance)
        {
            return "aba";
        }
    }

    if (snapshot.largeCandidateJump && snapshot.recentLargeCandidateCount >= 2)
        return "repeated_jump";

    const bool previousSteady = !m_jumpDiagPreFrames.empty() && m_jumpDiagPreFrames.back().guiding &&
        !m_jumpDiagPreFrames.back().paused && !m_jumpDiagPreFrames.back().settling;
    if (previousSteady && (!snapshot.addedContributors.empty() || !snapshot.removedContributors.empty()) &&
        sqrt(snapshot.aggregateDelta.X * snapshot.aggregateDelta.X + snapshot.aggregateDelta.Y * snapshot.aggregateDelta.Y) >
            threshold)
    {
        return "membership_jump";
    }

    return wxEmptyString;
}

void GuiderMultiStar2::JumpDiagEmitSnapshot(const JumpDiagFrameSnapshot& snapshot, const wxString& phase,
                                             unsigned int sequence) const
{
    const unsigned int episode = m_jumpDiagEpisodeId;
    const wxString& trigger = m_jumpDiagEpisodeReason;
    const double arcsecScale = snapshot.imageScaleKnown ? snapshot.imageScale : 0.0;
    MS2LOGF("MultiStar2JumpDiag: episode=%u trigger=%s phase=%s seq=%u frame=%u timeMs=%lld guideTime=%.3f "
            "state=%d guiding=%d paused=%d settling=%d outcome=%s reason=%s lockValid=%d lock=(%.3f,%.3f) "
            "scale=%.6f scaleKnown=%d prevAcceptedValid=%d prevAccepted=(%.3f,%.3f) candidateValid=%d candidate=(%.3f,%.3f)\n",
            episode, trigger, phase, sequence, snapshot.frameNumber, snapshot.timestampMs, snapshot.guidingTime,
            snapshot.guiderState, snapshot.guiding ? 1 : 0, snapshot.paused ? 1 : 0, snapshot.settling ? 1 : 0,
            snapshot.outcome, snapshot.reason, snapshot.lockValid ? 1 : 0, snapshot.lockPosition.X, snapshot.lockPosition.Y,
            snapshot.imageScale, snapshot.imageScaleKnown ? 1 : 0, snapshot.previousAcceptedValid ? 1 : 0,
            snapshot.previousAccepted.X, snapshot.previousAccepted.Y, snapshot.candidateValid ? 1 : 0, snapshot.candidate.X,
            snapshot.candidate.Y);
    MS2LOGF("MultiStar2JumpDiag: episode=%u trigger=%s phase=%s seq=%u frame=%u displayedValid=%d displayed=(%.3f,%.3f) "
            "returnedCameraValid=%d returnedCamera=(%.3f,%.3f) returnedMountValid=%d "
            "returnedMount=(%.3f,%.3f) normalHandoffPossible=%d deducedZero=%d "
            "baseValid=%d base=(%.3f,%.3f) aggregateDelta=(%.3f,%.3f) largeCandidateJump=%d "
            "recentLargeCandidates=%u jumpDistance=%.3f jumpState=%s jumpRejected=%d referenceRepinned=%d\n",
            episode, trigger, phase, sequence, snapshot.frameNumber, snapshot.displayedValid ? 1 : 0, snapshot.displayed.X,
            snapshot.displayed.Y,
            snapshot.returnedCameraValid ? 1 : 0, snapshot.returnedCameraOffset.X, snapshot.returnedCameraOffset.Y,
            snapshot.returnedMountValid ? 1 : 0, snapshot.returnedMountOffset.X, snapshot.returnedMountOffset.Y,
            snapshot.normalHandoffPossible ? 1 : 0, snapshot.deducedZeroMoveExpected ? 1 : 0,
            snapshot.baseDispValid ? 1 : 0, snapshot.baseDisp.X, snapshot.baseDisp.Y, snapshot.aggregateDelta.X,
            snapshot.aggregateDelta.Y, snapshot.largeCandidateJump ? 1 : 0, snapshot.recentLargeCandidateCount,
            snapshot.jumpDistance, snapshot.jumpState, snapshot.jumpRejected ? 1 : 0,
            snapshot.referenceRepinned ? 1 : 0);
    MS2LOGF("MultiStar2JumpDiag: episode=%u trigger=%s phase=%s seq=%u frame=%u "
            "prevAcceptedArcsec=(%.3f,%.3f) candidateArcsec=(%.3f,%.3f) displayedArcsec=(%.3f,%.3f) "
            "returnedCameraArcsec=(%.3f,%.3f) returnedMountArcsec=(%.3f,%.3f) aggregateDeltaArcsec=(%.3f,%.3f)\n",
            episode, trigger, phase, sequence, snapshot.frameNumber, snapshot.previousAccepted.X * arcsecScale,
            snapshot.previousAccepted.Y * arcsecScale, snapshot.candidate.X * arcsecScale, snapshot.candidate.Y * arcsecScale,
            snapshot.displayed.X * arcsecScale, snapshot.displayed.Y * arcsecScale,
            snapshot.returnedCameraOffset.X * arcsecScale, snapshot.returnedCameraOffset.Y * arcsecScale,
            snapshot.returnedMountOffset.X * arcsecScale, snapshot.returnedMountOffset.Y * arcsecScale,
            snapshot.aggregateDelta.X * arcsecScale,
            snapshot.aggregateDelta.Y * arcsecScale);

    wxString contributors;
    for (size_t i = 0; i < snapshot.contributorMask.size(); i++)
        contributors += wxString::Format("%s%u:%d", i ? "," : "", (unsigned int) i, snapshot.contributorMask[i] ? 1 : 0);
    wxString added;
    for (size_t i = 0; i < snapshot.addedContributors.size(); i++)
        added += wxString::Format("%s%u", i ? "," : "", snapshot.addedContributors[i]);
    wxString removed;
    for (size_t i = 0; i < snapshot.removedContributors.size(); i++)
        removed += wxString::Format("%s%u", i ? "," : "", snapshot.removedContributors[i]);
    MS2LOGF("MultiStar2JumpDiag: episode=%u trigger=%s phase=%s seq=%u frame=%u contributors=[%s] added=[%s] "
            "removed=[%s]\n",
            episode, trigger, phase, sequence, snapshot.frameNumber, contributors, added, removed);

    for (const auto& star : snapshot.stars)
    {
        MS2LOGF("MultiStar2JumpDiag: episode=%u trigger=%s phase=%s seq=%u frame=%u star=%u expectedValid=%d "
                "expected=(%.3f,%.3f) found=%d measured=(%.3f,%.3f) referenceValid=%d reference=(%.3f,%.3f) "
                "displacement=(%.3f,%.3f) snr=%.3f mass=%.3f adjustedMass=%.6f median=%.6f\n",
                episode, trigger, phase, sequence, snapshot.frameNumber, star.index, star.expectedValid ? 1 : 0, star.expected.X,
                star.expected.Y, star.found ? 1 : 0, star.measured.X, star.measured.Y, star.referenceValid ? 1 : 0,
                star.reference.X, star.reference.Y, star.displacement.X, star.displacement.Y, star.snr, star.mass,
                star.adjustedMass, star.massMedian);
        MS2LOGF("MultiStar2JumpDiag: episode=%u trigger=%s phase=%s seq=%u frame=%u star=%u "
                "massBoundsValid=%d bounds=(%.6f,%.6f) spike=%.6f massRejected=%d reacquire=%u "
                "eligibility=%s contributing=%d weight=%.3f repinned=%d oldRef=(%.3f,%.3f) "
                "newRef=(%.3f,%.3f) prePinDisp=(%.3f,%.3f) displacementArcsec=(%.3f,%.3f)\n",
                episode, trigger, phase, sequence, snapshot.frameNumber, star.index, star.massBoundsValid ? 1 : 0,
                star.massLowBound, star.massHighBound, star.massSpikeBound,
                star.massRejected ? 1 : 0, star.reacquireGoodCount, star.eligibilityReason, star.contributing ? 1 : 0,
                star.weight, star.repinned ? 1 : 0, star.oldReference.X, star.oldReference.Y, star.newReference.X,
                star.newReference.Y, star.prePinDisplacement.X, star.prePinDisplacement.Y, star.displacement.X * arcsecScale,
                star.displacement.Y * arcsecScale);
    }
}

void GuiderMultiStar2::JumpDiagProcess(const JumpDiagFrameSnapshot& snapshot)
{
    if (m_jumpDiagCollectingPost)
    {
        m_jumpDiagLargeCandidateTimes.clear();
        const unsigned int sequence = 11 - m_jumpDiagPostRemaining;
        JumpDiagEmitSnapshot(snapshot, "post", sequence);
        if (--m_jumpDiagPostRemaining == 0)
        {
            m_jumpDiagCollectingPost = false;
            m_jumpDiagCooldownUntil = snapshot.timestampMs + 60000;
        }
    }
    else if (snapshot.timestampMs < m_jumpDiagCooldownUntil)
    {
        m_jumpDiagLargeCandidateTimes.clear();
    }
    else if (m_jumpDiagPreFrames.size() >= 20)
    {
        const wxString trigger = JumpDiagTriggerReason(snapshot);
        if (!trigger.empty())
        {
            ++m_jumpDiagEpisodeId;
            m_jumpDiagEpisodeReason = trigger;
            // TODO(multistar2-jump-diagnostics): TEMPORARY
            pFrame->Alert(wxString::Format("Multistar2 tracking event detected (episode %u, trigger %s, frame %u). "
                                           "Check the debug log for details.",
                                           m_jumpDiagEpisodeId, m_jumpDiagEpisodeReason, snapshot.frameNumber));
            unsigned int sequence = 1;
            const size_t first = m_jumpDiagPreFrames.size() > 20 ? m_jumpDiagPreFrames.size() - 20 : 0;
            for (size_t i = first; i < m_jumpDiagPreFrames.size(); i++)
                JumpDiagEmitSnapshot(m_jumpDiagPreFrames[i], "pre", sequence++);
            JumpDiagEmitSnapshot(snapshot, "event", 0);
            m_jumpDiagCollectingPost = true;
            m_jumpDiagPostRemaining = 10;
            m_jumpDiagLargeCandidateTimes.clear();
        }
    }

    m_jumpDiagPreFrames.push_back(snapshot);
    while (m_jumpDiagPreFrames.size() > 20)
        m_jumpDiagPreFrames.pop_front();
}
#endif

GuiderMultiStar2::GuiderMultiStar2(wxWindow *parent) : GuiderMultiStar(parent)
{
    // Ensure our override is actually used for paint events. GuiderMultiStar registers its own
    // EVT_PAINT handler via an event table, which does not virtual-dispatch to overrides.
    Bind(wxEVT_PAINT, &GuiderMultiStar2::OnPaint, this);
#if MULTISTAR2_JUMP_DIAGNOSTICS
    // TODO(multistar2-jump-diagnostics): TEMPORARY
    JumpDiagReset();
#endif
}
 
GuiderMultiStar2::~GuiderMultiStar2() { }
 
wxString GuiderMultiStar2::GetSettingsSummary() const
{
    // Keep multistar summary, but tag the implementation for log visibility.
    wxString s = GuiderMultiStar::GetSettingsSummary();
    if (!s.empty())
        s = _("MultiStar2: ") + s;
    return s;
}

void GuiderMultiStar2::SetDroppedFrameInfo(const usImage *pImage, FrameDroppedInfo *errorInfo, const wxString& status, double mass,
                                           double snr, double hfd, bool setStatusMsg, bool resetAutoExposure) const
{
    errorInfo->starError = Star::STAR_ERROR;
    errorInfo->starMass = mass;
    errorInfo->starSNR = snr;
    errorInfo->starHFD = hfd;
    errorInfo->status = status;
    if (setStatusMsg)
        pFrame->StatusMsg(status);

    ImageLogger::LogImage(pImage, *errorInfo);
    if (resetAutoExposure)
        pFrame->ResetAutoExposure();
}

void GuiderMultiStar2::EnsureStarStateSize()
{
    if (m_starState.size() != m_guideStars.size())
        m_starState.resize(m_guideStars.size());
}

bool GuiderMultiStar2::IsLocked() const
{
    return m_solutionStar.WasFound();
}

const PHD_Point& GuiderMultiStar2::CurrentPosition() const
{
    return m_solutionStar;
}

const Star& GuiderMultiStar2::PrimaryStar() const
{
    // For UI/status reporting, prefer a real found star with Mass/SNR/HFD.
    return m_displayStar.WasFound() ? m_displayStar : m_primaryStar;
}

wxString GuiderMultiStar2::GetStarCount() const
{
    // Show contributing / max concurrently contributing stars since (re)select.
    return wxString::Format("%u/%u", m_solutionStarsUsed, m_maxConcurrentStarsUsed);
}

void GuiderMultiStar2::InvalidateCurrentPosition(bool fullReset)
{
    // Mirror GuiderMultiStar::InvalidateCurrentPosition (can't call it directly; it's private there)
    m_primaryStar.Invalidate();
    if (fullReset)
    {
        m_primaryStar.X = m_primaryStar.Y = 0.0;
    }
    m_solutionStar.Invalidate();
    m_displayStar.Invalidate();
    m_solutionStarsUsed = 0;
    m_maxConcurrentStarsUsed = 0;
    m_starState.clear();
#if MULTISTAR2_JUMP_DIAGNOSTICS
    // TODO(multistar2-jump-diagnostics): TEMPORARY
    JumpDiagReset();
#endif
}

bool GuiderMultiStar2::UpdateCurrentPosition(const usImage *pImage, GuiderOffset *ofs, FrameDroppedInfo *errorInfo)
{
    // Compute the guiding offset from per-star displacements relative to each star's reference point.
    // This preserves continuity (no systematic step) when stars are lost or re-acquired.

    static const unsigned int REACQUIRE_GOOD_FRAMES = 3;
    const unsigned int poolSize = (unsigned int) m_guideStars.size();
    unsigned int rejectNotFound = 0;
    unsigned int rejectMass = 0;
    unsigned int rejectReacquireGate = 0;

#if MULTISTAR2_JUMP_DIAGNOSTICS
    // TODO(multistar2-jump-diagnostics): TEMPORARY
    // Capture is observation-only; FinalizeJumpDiag is called immediately before every return.
    JumpDiagFrameSnapshot jumpDiag;
    auto InitDiagPoint = [](PHD_Point& point) { point.SetXY(0.0, 0.0); };
    jumpDiag.frameNumber = pImage->FrameNum;
    jumpDiag.timestampMs = ::wxGetUTCTimeMillis().GetValue();
    jumpDiag.guidingTime = pFrame->TimeSinceGuidingStarted();
    jumpDiag.guiderState = (int) GetState();
    jumpDiag.guiding = GetState() == STATE_GUIDING;
    jumpDiag.paused = IsPaused();
    jumpDiag.settling = PhdController::IsSettling();
    jumpDiag.lockValid = LockPosition().IsValid();
    InitDiagPoint(jumpDiag.lockPosition);
    if (jumpDiag.lockValid)
        jumpDiag.lockPosition = LockPosition();
    jumpDiag.imageScale = pFrame->GetCameraPixelScale();
    jumpDiag.imageScaleKnown = jumpDiag.imageScale > 0.0 && jumpDiag.imageScale != 1.0;
    jumpDiag.previousAcceptedValid = m_jumpDiagLastAcceptedValid;
    InitDiagPoint(jumpDiag.previousAccepted);
    if (jumpDiag.previousAcceptedValid)
        jumpDiag.previousAccepted = m_jumpDiagLastAcceptedDisp;
    InitDiagPoint(jumpDiag.candidate);
    InitDiagPoint(jumpDiag.displayed);
    InitDiagPoint(jumpDiag.returnedCameraOffset);
    InitDiagPoint(jumpDiag.returnedMountOffset);
    InitDiagPoint(jumpDiag.baseDisp);
    jumpDiag.aggregateDelta.SetXY(0.0, 0.0);
    jumpDiag.contributorMask.assign(poolSize, false);
    jumpDiag.stars.resize(poolSize);
    for (size_t i = 0; i < jumpDiag.stars.size(); i++)
    {
        JumpDiagStarSnapshot& star = jumpDiag.stars[i];
        star.index = (unsigned int) i;
        InitDiagPoint(star.expected);
        InitDiagPoint(star.measured);
        InitDiagPoint(star.reference);
        InitDiagPoint(star.displacement);
        InitDiagPoint(star.oldReference);
        InitDiagPoint(star.newReference);
        InitDiagPoint(star.prePinDisplacement);
        if (m_guideStars[i].referencePoint.IsValid())
        {
            star.referenceValid = true;
            star.reference = m_guideStars[i].referencePoint;
        }
        star.reacquireGoodCount = m_starState.size() > i ? m_starState[i].reacquireGoodCount : 0;
        star.eligibilityReason = "not_examined";
    }
    auto FinalizeJumpDiag = [&](const wxString& outcome, const wxString& reason, bool success) {
        jumpDiag.outcome = outcome;
        jumpDiag.reason = reason;
        jumpDiag.deducedZeroMoveExpected = !success && GetState() == STATE_GUIDING;
        // The caller may still choose recenter/measurement handling; the GuideLog confirms the actual handoff.
        jumpDiag.normalHandoffPossible = success && GetState() == STATE_GUIDING && !IsPaused() && !IsRecentering();
        jumpDiag.addedContributors.clear();
        jumpDiag.removedContributors.clear();
        if (!m_jumpDiagPreFrames.empty() && m_jumpDiagPreFrames.back().contributorMask.size() == jumpDiag.contributorMask.size())
        {
            const std::vector<bool>& previousMask = m_jumpDiagPreFrames.back().contributorMask;
            for (size_t i = 0; i < jumpDiag.contributorMask.size(); i++)
            {
                if (jumpDiag.contributorMask[i] != previousMask[i])
                    (jumpDiag.contributorMask[i] ? jumpDiag.addedContributors : jumpDiag.removedContributors)
                        .push_back((unsigned int) i);
            }
        }
        if (success)
        {
            if (ofs->cameraOfs.IsValid())
            {
                jumpDiag.returnedCameraValid = true;
                jumpDiag.returnedCameraOffset = ofs->cameraOfs;
            }
            if (ofs->mountOfs.IsValid())
            {
                jumpDiag.returnedMountValid = true;
                jumpDiag.returnedMountOffset = ofs->mountOfs;
            }
        }
        const bool steadyGuiding = jumpDiag.guiding && !jumpDiag.paused && !jumpDiag.settling;
        if (!steadyGuiding)
            m_jumpDiagLargeCandidateTimes.clear();
        while (steadyGuiding && !m_jumpDiagLargeCandidateTimes.empty() &&
               jumpDiag.timestampMs - m_jumpDiagLargeCandidateTimes.front() > 20000)
        {
            m_jumpDiagLargeCandidateTimes.pop_front();
        }
        if (steadyGuiding && jumpDiag.candidateValid && jumpDiag.previousAcceptedValid)
        {
            const double dx = jumpDiag.candidate.X - jumpDiag.previousAccepted.X;
            const double dy = jumpDiag.candidate.Y - jumpDiag.previousAccepted.Y;
            const double threshold = jumpDiag.imageScaleKnown ? 2.0 / jumpDiag.imageScale : 1.0;
            jumpDiag.largeCandidateJump = sqrt(dx * dx + dy * dy) > threshold;
            if (jumpDiag.largeCandidateJump)
                m_jumpDiagLargeCandidateTimes.push_back(jumpDiag.timestampMs);
        }
        jumpDiag.recentLargeCandidateCount = (unsigned int) m_jumpDiagLargeCandidateTimes.size();
        JumpDiagProcess(jumpDiag);
        if (success && jumpDiag.candidateValid)
        {
            m_jumpDiagLastAcceptedDisp = jumpDiag.candidate;
            m_jumpDiagLastAcceptedValid = true;
        }
    };
#endif

    auto JoinUnsigned = [](const std::vector<unsigned int>& vals) -> wxString {
        wxString s;
        for (size_t i = 0; i < vals.size(); i++)
            s += wxString::Format("%s%u", i ? "," : "", vals[i]);
        return s;
    };
    auto EmitFrameSummary = [&](const wxString& outcome, const wxString& reason, unsigned int foundCount, unsigned int usedCount,
                                bool primaryContrib, double distance, const PHD_Point& disp, const PHD_Point& dDisp,
                                bool jumpRejected, const wxString& usedIdxStr, const wxString& addedStr,
                                const wxString& removedStr) {
        MS2LOGF("MultiStar2: frame outcome=%s reason=%s pool=%u found=%u used=%u primaryContrib=%d "
                "dist=%.3f disp=(%.3f,%.3f) dDisp=(%.3f,%.3f) notFound=%u mass=%u reacquireGate=%u jump=%d "
                "usedIdx=[%s] added=[%s] removed=[%s]\n",
                outcome, reason, poolSize, foundCount, usedCount, primaryContrib ? 1 : 0, distance, disp.X, disp.Y, dDisp.X,
                dDisp.Y, rejectNotFound, rejectMass, rejectReacquireGate, jumpRejected ? 1 : 0, usedIdxStr, addedStr,
                removedStr);
    };
    auto EmitRejectBreakdown = [&](const wxString& reason, unsigned int foundCount, unsigned int usedCount, bool jumpRejected,
                                   const wxString& usedIdxStr, const wxString& addedStr, const wxString& removedStr) {
        MS2LOGF("MultiStar2: reject reason=%s pool=%u found=%u used=%u notFound=%u mass=%u reacquireGate=%u jump=%d "
                "usedIdx=[%s] added=[%s] removed=[%s]\n",
                reason, poolSize, foundCount, usedCount, rejectNotFound, rejectMass, rejectReacquireGate,
                jumpRejected ? 1 : 0, usedIdxStr, addedStr, removedStr);
    };

    const Star prevSolution(m_solutionStar);
    const PHD_Point prevDisp = (prevSolution.WasFound() && LockPosition().IsValid()) ? (prevSolution - LockPosition())
                                                                                     : PHD_Point(0.0, 0.0);

    m_solutionStar.Invalidate();
    m_displayStar.Invalidate();
    m_solutionStarsUsed = 0;
    EnsureStarStateSize();
    for (auto& st : m_starState)
    {
        st.foundThisFrame = false;
        st.contributingThisFrame = false;
    }

    // No star selected / no list
    if (!m_primaryStar.IsValid() && m_primaryStar.X == 0.0 && m_primaryStar.Y == 0.0)
    {
        errorInfo->starError = Star::STAR_ERROR;
        errorInfo->starMass = 0.0;
        errorInfo->starSNR = 0.0;
        errorInfo->starHFD = 0.0;
        errorInfo->status = _("No star selected");
        ImageLogger::LogImageStarDeselected(pImage);
        EmitFrameSummary("drop", "no_star_selected", 0, 0, false, 0.0, PHD_Point(0.0, 0.0), PHD_Point(0.0, 0.0), false, "", "",
                         "");
        EmitRejectBreakdown("no_star_selected", 0, 0, false, "", "", "");
#if MULTISTAR2_JUMP_DIAGNOSTICS
        // TODO(multistar2-jump-diagnostics): TEMPORARY
        FinalizeJumpDiag("drop", "no_star_selected", false);
#endif
        return true;
    }

    const PHD_Point& lockPos = LockPosition();
    bool raOnly = MyFrame::GuidingRAOnly();

    // Mass-change normalization (match multistar behavior: normalize by exposure when auto-exposure is enabled)
    int exposureMs = 0;
    bool isAutoExp = false;
    pFrame->GetExposureInfo(&exposureMs, &isAutoExp);
    auto adjMass = [&](double mass) -> double {
        if (isAutoExp && exposureMs > 0)
            return mass / (double) exposureMs;
        return mass;
    };

    auto massReject = [&](StarState& st, double mass) -> bool {
#if MULTISTAR2_JUMP_DIAGNOSTICS
        // TODO(multistar2-jump-diagnostics): TEMPORARY
        JumpDiagStarSnapshot& diagStar = jumpDiag.stars[(size_t) (&st - &m_starState[0])];
        diagStar.adjustedMass = adjMass(mass);
#endif
        if (!m_massChangeThresholdEnabled)
            return false;

        // Keep a simple time window, similar spirit to MassChecker
        wxLongLong_t now = ::wxGetUTCTimeMillis().GetValue();
        wxLongLong_t oldest = now - 22500 * 2; // DefaultTimeWindowMs * 2 in multistar

        while (!st.massHist.empty() && st.massHist.front().first < oldest)
            st.massHist.pop_front();

        double am = adjMass(mass);
        st.massHist.push_back({ now, am });

        if (st.massHist.size() < 5)
            return false;

        std::vector<double> tmp;
        tmp.reserve(st.massHist.size());
        for (const auto& e : st.massHist)
            tmp.push_back(e.second);

        size_t mid = tmp.size() / 2;
        std::nth_element(tmp.begin(), tmp.begin() + mid, tmp.end());
        double med = tmp[mid];

        if (med > st.highMass)
            st.highMass = med;
        if (med < st.lowMass)
            st.lowMass = med;
        st.lowMass += .05 * (med - st.lowMass); // drift low-water mark upward

        double low = st.lowMass * (1. - m_massChangeThreshold);
        double high = st.highMass * (1. + m_massChangeThreshold);
        double spike = med * (1. + 2.0 * m_massChangeThreshold);

#if MULTISTAR2_JUMP_DIAGNOSTICS
        // TODO(multistar2-jump-diagnostics): TEMPORARY
        diagStar.massMedian = med;
        diagStar.massLowBound = low;
        diagStar.massHighBound = high;
        diagStar.massSpikeBound = spike;
        diagStar.massBoundsValid = true;
#endif
        return am < low || am > high || am > spike;
    };

    struct Found
    {
        size_t idx;
        Star star;
        bool eligible; // found + passes mass + passes gating
    };
    std::vector<Found> found;
    found.reserve(m_guideStars.size());

    // Determine whether we can use secondaries (respect subframes behavior)
    bool allowSecondaries = m_multiStarMode && m_guideStars.size() > 1 && !pCamera->UseSubframes;

    // Use last solution as reference primary estimate for searching lost secondaries
    PHD_Point refPrimary = prevSolution.WasFound() ? static_cast<const PHD_Point&>(prevSolution)
                                                   : static_cast<const PHD_Point&>(m_primaryStar);

    // Find/update each star in m_guideStars (index 0 is primary)
    for (size_t i = 0; i < m_guideStars.size(); i++)
    {
        if (i > 0 && !allowSecondaries)
            break;

        GuideStar& gs = m_guideStars[i];
        StarState& st = m_starState[i];

        GuideStar s(gs);
        bool ok = false;

#if MULTISTAR2_JUMP_DIAGNOSTICS
        // TODO(multistar2-jump-diagnostics): TEMPORARY
        JumpDiagStarSnapshot& diagStar = jumpDiag.stars[i];
        diagStar.index = (unsigned int) i;
        if (gs.referencePoint.IsValid())
        {
            diagStar.referenceValid = true;
            diagStar.reference = gs.referencePoint;
        }
        diagStar.reacquireGoodCount = st.reacquireGoodCount;
        if (st.lastPosValid)
        {
            diagStar.expectedValid = true;
            diagStar.expected = st.lastPos;
        }
        else if (i > 0)
        {
            diagStar.expectedValid = true;
            diagStar.expected = refPrimary + gs.offsetFromPrimary;
        }
        else
        {
            diagStar.expectedValid = true;
            diagStar.expected.SetXY(s.X, s.Y);
        }
#endif

        if (st.lastPosValid)
        {
            ok = s.Find(pImage, m_searchRegion, st.lastPos.X, st.lastPos.Y, pFrame->GetStarFindMode(), GetMinStarHFD(),
                        GetMaxStarHFD(), pCamera->GetSaturationADU(), Star::FIND_LOGGING_MINIMAL);
        }
        else if (i == 0)
        {
            ok = s.Find(pImage, m_searchRegion, pFrame->GetStarFindMode(), GetMinStarHFD(), GetMaxStarHFD(),
                        pCamera->GetSaturationADU(), Star::FIND_LOGGING_VERBOSE);
        }
        else
        {
            PHD_Point expected = refPrimary + gs.offsetFromPrimary;
            ok = s.Find(pImage, m_searchRegion, expected.X, expected.Y, pFrame->GetStarFindMode(), GetMinStarHFD(), GetMaxStarHFD(),
                        pCamera->GetSaturationADU(), Star::FIND_LOGGING_MINIMAL);
        }

        st.foundThisFrame = ok;
        if (!ok)
        {
            gs.wasLost = true;
            st.reacquireGoodCount = 0;
            rejectNotFound++;
#if MULTISTAR2_JUMP_DIAGNOSTICS
            // TODO(multistar2-jump-diagnostics): TEMPORARY
            diagStar.eligibilityReason = "not_found";
            diagStar.reacquireGoodCount = 0;
#endif
            continue;
        }

        // Update star record + last-known position
        gs.X = s.X;
        gs.Y = s.Y;
        gs.Mass = s.Mass;
        gs.SNR = s.SNR;
        gs.HFD = s.HFD;
        gs.PeakVal = s.PeakVal;
        st.lastPos.SetXY(s.X, s.Y);
        st.lastPosValid = true;

        // reacquire gating
        if (gs.wasLost)
            st.reacquireGoodCount++;
        else
            st.reacquireGoodCount = REACQUIRE_GOOD_FRAMES; // stable by default

        // mass-change rejection (per star)
        bool reject = massReject(st, s.Mass);
        if (reject)
            rejectMass++;

        bool gatedIn = st.reacquireGoodCount >= REACQUIRE_GOOD_FRAMES;
        if (!gatedIn)
            rejectReacquireGate++;
        bool eligible = gatedIn && !reject;

#if MULTISTAR2_JUMP_DIAGNOSTICS
        // TODO(multistar2-jump-diagnostics): TEMPORARY
        diagStar.found = true;
        diagStar.measured.SetXY(s.X, s.Y);
        if (gs.referencePoint.IsValid())
            diagStar.displacement.SetXY(s.X - gs.referencePoint.X, s.Y - gs.referencePoint.Y);
        diagStar.snr = s.SNR;
        diagStar.mass = s.Mass;
        diagStar.massRejected = reject;
        diagStar.reacquireGoodCount = st.reacquireGoodCount;
        diagStar.eligibilityReason = reject ? "mass_rejected" : (!gatedIn ? "reacquire_gate" : "eligible");
#endif

        // Mark as "found but not contributing yet" if gated/rejected
        found.push_back({ i, s, eligible });
    }

#if MULTISTAR2_JUMP_DIAGNOSTICS
    // TODO(multistar2-jump-diagnostics): TEMPORARY
    for (size_t i = allowSecondaries ? m_guideStars.size() : wxMin((size_t) 1, m_guideStars.size()); i < m_guideStars.size(); i++)
    {
        jumpDiag.stars[i].index = (unsigned int) i;
        if (m_guideStars[i].referencePoint.IsValid())
        {
            jumpDiag.stars[i].referenceValid = true;
            jumpDiag.stars[i].reference = m_guideStars[i].referencePoint;
        }
        jumpDiag.stars[i].reacquireGoodCount = m_starState[i].reacquireGoodCount;
        jumpDiag.stars[i].eligibilityReason = "secondaries_disabled";
    }
#endif

    if (found.empty())
    {
        SetDroppedFrameInfo(pImage, errorInfo, _("Star lost"), 0.0, 0.0, 0.0, false, true);
#if MULTISTAR2_JUMP_DIAGNOSTICS
        // TODO(multistar2-jump-diagnostics): TEMPORARY
        const DistanceChecker2::State jumpStateBeforeActivate = s_distanceChecker2.CurrentState();
#endif
        s_distanceChecker2.Activate();
        // Use max exposure duration while no usable stars are available.
        EmitFrameSummary("drop", "all_lost", (unsigned int) found.size(), 0, false, 0.0, prevDisp, PHD_Point(0.0, 0.0), false,
                         "", "", "");
        EmitRejectBreakdown("all_lost", (unsigned int) found.size(), 0, false, "", "", "");
#if MULTISTAR2_JUMP_DIAGNOSTICS
        // TODO(multistar2-jump-diagnostics): TEMPORARY
        jumpDiag.jumpState = wxString::Format("before=%s,event=activate,after=%s", DistanceChecker2::StateName(
                                                  jumpStateBeforeActivate),
                                              DistanceChecker2::StateName(s_distanceChecker2.CurrentState()));
        FinalizeJumpDiag("drop", "all_lost", false);
#endif
        return true;
    }

    // Determine base displacement to preserve continuity when adding stars
    PHD_Point baseDisp = prevDisp;
    {
        double sumW = 0.0, sumDX = 0.0, sumDY = 0.0;
        for (const auto& f : found)
        {
            if (!f.eligible)
                continue;
            const GuideStar& gs = m_guideStars[f.idx];
            double w = f.star.SNR > 0.0 ? f.star.SNR : 1.0;
            sumW += w;
            sumDX += w * (f.star.X - gs.referencePoint.X);
            sumDY += w * (f.star.Y - gs.referencePoint.Y);
        }
        if (sumW > 0.0)
            baseDisp.SetXY(sumDX / sumW, sumDY / sumW);
    }
#if MULTISTAR2_JUMP_DIAGNOSTICS
    // TODO(multistar2-jump-diagnostics): TEMPORARY
    jumpDiag.baseDispValid = true;
    jumpDiag.baseDisp = baseDisp;
#endif

    // For stars transitioning into "eligible", pin their referencePoint so they don't pull the solution
    for (const auto& f : found)
    {
        if (!f.eligible)
            continue;
        GuideStar& gs = m_guideStars[f.idx];
        if (gs.wasLost && m_starState[f.idx].reacquireGoodCount == REACQUIRE_GOOD_FRAMES)
        {
#if MULTISTAR2_DEBUG_LOG
            const PHD_Point oldRef = gs.referencePoint;
#endif
#if MULTISTAR2_JUMP_DIAGNOSTICS
            // TODO(multistar2-jump-diagnostics): TEMPORARY
            JumpDiagStarSnapshot& diagStar = jumpDiag.stars[f.idx];
            diagStar.repinned = true;
            if (gs.referencePoint.IsValid())
            {
                diagStar.oldReference = gs.referencePoint;
                diagStar.prePinDisplacement.SetXY(f.star.X - gs.referencePoint.X, f.star.Y - gs.referencePoint.Y);
            }
            else
                diagStar.prePinDisplacement = baseDisp;
            diagStar.eligibilityReason += "|reacquire_repin";
#endif
            gs.referencePoint.X = f.star.X - baseDisp.X;
            gs.referencePoint.Y = f.star.Y - baseDisp.Y;
            gs.wasLost = false;

#if MULTISTAR2_JUMP_DIAGNOSTICS
            // TODO(multistar2-jump-diagnostics): TEMPORARY
            diagStar.newReference = gs.referencePoint;
            diagStar.displacement.SetXY(f.star.X - gs.referencePoint.X, f.star.Y - gs.referencePoint.Y);
            jumpDiag.referenceRepinned = true;
#endif
#if MULTISTAR2_DEBUG_LOG
            MS2LOGF("MultiStar2: reacquire idx=%u reacqGood=%u star=(%.2f,%.2f) baseDisp=(%.3f,%.3f) ref: (%.3f,%.3f)->(%.3f,%.3f)\n",
                    (unsigned int) f.idx, m_starState[f.idx].reacquireGoodCount, f.star.X, f.star.Y, baseDisp.X, baseDisp.Y,
                    oldRef.X, oldRef.Y, gs.referencePoint.X, gs.referencePoint.Y);
#endif
        }
        else if (!gs.referencePoint.IsValid())
        {
#if MULTISTAR2_JUMP_DIAGNOSTICS
            // TODO(multistar2-jump-diagnostics): TEMPORARY
            JumpDiagStarSnapshot& diagStar = jumpDiag.stars[f.idx];
            diagStar.repinned = true;
            diagStar.prePinDisplacement = baseDisp;
            diagStar.eligibilityReason += "|invalid_reference_repin";
#endif
            gs.referencePoint.X = f.star.X - baseDisp.X;
            gs.referencePoint.Y = f.star.Y - baseDisp.Y;
            gs.wasLost = false;
#if MULTISTAR2_JUMP_DIAGNOSTICS
            // TODO(multistar2-jump-diagnostics): TEMPORARY
            diagStar.newReference = gs.referencePoint;
            diagStar.displacement.SetXY(f.star.X - gs.referencePoint.X, f.star.Y - gs.referencePoint.Y);
            jumpDiag.referenceRepinned = true;
#endif
        }
        else
        {
            gs.wasLost = false;
        }
    }

    // Compute final displacement from all eligible stars
    double sumW = 0.0, sumDX = 0.0, sumDY = 0.0;
    size_t best = (size_t) -1;
    for (const auto& f : found)
    {
        if (!f.eligible)
            continue;
        const GuideStar& gs = m_guideStars[f.idx];
        double w = f.star.SNR > 0.0 ? f.star.SNR : 1.0;
        sumW += w;
        sumDX += w * (f.star.X - gs.referencePoint.X);
        sumDY += w * (f.star.Y - gs.referencePoint.Y);
        m_starState[f.idx].contributingThisFrame = true;
#if MULTISTAR2_JUMP_DIAGNOSTICS
        // TODO(multistar2-jump-diagnostics): TEMPORARY
        jumpDiag.stars[f.idx].contributing = true;
        jumpDiag.stars[f.idx].weight = w;
        jumpDiag.contributorMask[f.idx] = true;
#endif
        if (best == (size_t) -1 || f.star.SNR > found[best].star.SNR)
            best = &f - &found[0];
    }

    unsigned int contributing = 0;
    std::vector<unsigned int> usedIdx;
    usedIdx.reserve(poolSize);
    for (unsigned int i = 0; i < (unsigned int) m_starState.size(); i++)
        if (m_starState[i].contributingThisFrame)
        {
            contributing++;
            usedIdx.push_back(i);
        }
    if (contributing == 0)
    {
        size_t bestFound = 0;
        for (size_t i = 1; i < found.size(); i++)
            if (found[i].star.SNR > found[bestFound].star.SNR)
                bestFound = i;
        m_displayStar = found[bestFound].star;

        SetDroppedFrameInfo(pImage, errorInfo, _("Recovering"), m_displayStar.Mass, m_displayStar.SNR, m_displayStar.HFD, true,
                            true);
#if MULTISTAR2_JUMP_DIAGNOSTICS
        // TODO(multistar2-jump-diagnostics): TEMPORARY
        const DistanceChecker2::State jumpStateBeforeActivate = s_distanceChecker2.CurrentState();
#endif
        s_distanceChecker2.Activate();
        // Use max exposure duration while recovering from unusable contributors.
        EmitFrameSummary("recovering", "no_contributors", (unsigned int) found.size(), contributing, false, 0.0, prevDisp,
                         PHD_Point(0.0, 0.0), false, "", "", "");
        EmitRejectBreakdown("no_contributors", (unsigned int) found.size(), contributing, false, "", "", "");
#if MULTISTAR2_JUMP_DIAGNOSTICS
        // TODO(multistar2-jump-diagnostics): TEMPORARY
        jumpDiag.jumpState =
            wxString::Format("before=%s,event=activate,after=%s", DistanceChecker2::StateName(jumpStateBeforeActivate),
                             DistanceChecker2::StateName(s_distanceChecker2.CurrentState()));
        FinalizeJumpDiag("recovering", "no_contributors", false);
#endif
        return true;
    }

    PHD_Point disp = baseDisp;
    if (sumW > 0.0)
        disp.SetXY(sumDX / sumW, sumDY / sumW);

#if MULTISTAR2_JUMP_DIAGNOSTICS
    // TODO(multistar2-jump-diagnostics): TEMPORARY
    jumpDiag.candidateValid = true;
    jumpDiag.candidate = disp;
    jumpDiag.aggregateDelta =
        !m_jumpDiagPreFrames.empty() && m_jumpDiagPreFrames.back().candidateValid ? disp - m_jumpDiagPreFrames.back().candidate
                                                                                  : disp - prevDisp;
#endif

    // Build solution star from lockPos + displacement
    if (lockPos.IsValid())
        m_solutionStar.SetXY(lockPos.X + disp.X, lockPos.Y + disp.Y);
    else
        m_solutionStar.SetXY(m_primaryStar.X + disp.X, m_primaryStar.Y + disp.Y);

    m_solutionStar.SetError(Star::STAR_OK);

    // Contributing count and session max
    m_solutionStarsUsed = contributing;
    if (contributing > m_maxConcurrentStarsUsed)
        m_maxConcurrentStarsUsed = contributing;

    wxString addedStr;
    wxString removedStr;
#if MULTISTAR2_DEBUG_LOG
    {
        const unsigned int foundCount = (unsigned int) found.size(); // found (may include gated/rejected)
        const unsigned int usedCount = m_solutionStarsUsed; // eligible + used this frame
        const bool primaryContrib = !m_starState.empty() && m_starState[0].contributingThisFrame;

        if (!m_dbgInited)
        {
            m_dbgInited = true;
            m_dbgLastPoolSize = poolSize;
            m_dbgLastFoundCount = foundCount;
            m_dbgLastUsedCount = usedCount;
            m_dbgLastPrimaryContrib = primaryContrib;
            m_dbgLastDisp = prevDisp;
            m_dbgLastContribMask.assign(poolSize, false);
        }

        std::vector<unsigned int> added;
        std::vector<unsigned int> removed;
        added.reserve(poolSize);
        removed.reserve(poolSize);
        if (m_dbgLastContribMask.size() != poolSize)
            m_dbgLastContribMask.assign(poolSize, false);
        for (unsigned int i = 0; i < poolSize; i++)
        {
            bool now = m_starState[i].contributingThisFrame;
            bool was = m_dbgLastContribMask[i];
            if (now != was)
            {
                (now ? added : removed).push_back(i);
                m_dbgLastContribMask[i] = now;
            }
        }
        addedStr = JoinUnsigned(added);
        removedStr = JoinUnsigned(removed);

#if MULTISTAR2_JUMP_DIAGNOSTICS
        // TODO(multistar2-jump-diagnostics): TEMPORARY
        jumpDiag.addedContributors = added;
        jumpDiag.removedContributors = removed;
#endif

        const bool anyMembershipChange = !added.empty() || !removed.empty();
        const bool anySummaryChange = (poolSize != m_dbgLastPoolSize) || (foundCount != m_dbgLastFoundCount) ||
            (usedCount != m_dbgLastUsedCount) || (primaryContrib != m_dbgLastPrimaryContrib);

        if (anyMembershipChange || anySummaryChange)
        {
            const PHD_Point dDisp = disp - m_dbgLastDisp;
            const PHD_Point& lockPosForLog = LockPosition();
            MS2LOGF("MultiStar2: pool=%u found=%u used=%u primaryContrib=%d added=[%s] removed=[%s] "
                    "disp=(%.3f,%.3f) dDisp=(%.3f,%.3f) lock=(%.3f,%.3f) sol=(%.3f,%.3f)\n",
                    poolSize, foundCount, usedCount, primaryContrib ? 1 : 0, addedStr, removedStr, disp.X, disp.Y, dDisp.X,
                    dDisp.Y, lockPosForLog.X, lockPosForLog.Y, m_solutionStar.X, m_solutionStar.Y);

            m_dbgLastPoolSize = poolSize;
            m_dbgLastFoundCount = foundCount;
            m_dbgLastUsedCount = usedCount;
            m_dbgLastPrimaryContrib = primaryContrib;
            m_dbgLastDisp = disp;
        }
    }
#endif

    // Choose display star: best eligible if possible, else best found
    if (best != (size_t) -1)
        m_displayStar = found[best].star;
    else
    {
        size_t bestFound = 0;
        for (size_t i = 1; i < found.size(); i++)
            if (found[i].star.SNR > found[bestFound].star.SNR)
                bestFound = i;
        m_displayStar = found[bestFound].star;
    }

    // Populate status fields from the display star
    m_solutionStar.Mass = m_displayStar.Mass;
    m_solutionStar.SNR = m_displayStar.SNR;
    m_solutionStar.HFD = m_displayStar.HFD;
    m_solutionStar.PeakVal = m_displayStar.PeakVal;

#if MULTISTAR2_JUMP_DIAGNOSTICS
    // TODO(multistar2-jump-diagnostics): TEMPORARY
    // "displayed" is the aggregate solution displacement, not the profile/UI star.
    if (lockPos.IsValid())
    {
        jumpDiag.displayedValid = true;
        jumpDiag.displayed = m_solutionStar - lockPos;
    }
#endif

    // Compute offsets vs lock position (as in multistar)
    double distance = 0.;
    double distanceRA = 0.;
    if (lockPos.IsValid())
    {
        ofs->cameraOfs = m_solutionStar - lockPos;

        if (pMount && pMount->IsCalibrated())
            pMount->TransformCameraCoordinatesToMountCoordinates(ofs->cameraOfs, ofs->mountOfs, true);

        distance = raOnly ? fabs(m_solutionStar.X - lockPos.X) : m_solutionStar.Distance(lockPos);
        distanceRA = ofs->mountOfs.IsValid() ? fabs(ofs->mountOfs.X) : 0.;
    }

    double tolerance = m_tolerateJumpsEnabled ? m_tolerateJumpsThreshold : 9e99;
#if MULTISTAR2_JUMP_DIAGNOSTICS
    // TODO(multistar2-jump-diagnostics): TEMPORARY
    const DistanceChecker2::State jumpStateBeforeCheck = s_distanceChecker2.CurrentState();
    jumpDiag.jumpDistance = distance;
#endif
    if (!s_distanceChecker2.CheckDistance(distance, raOnly, tolerance))
    {
        SetDroppedFrameInfo(pImage, errorInfo, _("Recovering"), m_displayStar.Mass, m_displayStar.SNR, m_displayStar.HFD, true,
                            true);
        // Use max exposure duration while recovering from large offsets.
        bool primaryContrib = !m_starState.empty() && m_starState[0].contributingThisFrame;
        PHD_Point dDisp = disp - prevDisp;
        wxString usedIdxStr = JoinUnsigned(usedIdx);
        EmitFrameSummary("recovering", "jump_reject", (unsigned int) found.size(), contributing, primaryContrib, distance, disp,
                         dDisp, true, usedIdxStr, addedStr, removedStr);
        EmitRejectBreakdown("jump_reject", (unsigned int) found.size(), contributing, true, usedIdxStr, addedStr, removedStr);
#if MULTISTAR2_JUMP_DIAGNOSTICS
        // TODO(multistar2-jump-diagnostics): TEMPORARY
        jumpDiag.jumpRejected = true;
        jumpDiag.jumpState =
            wxString::Format("before=%s,check=reject,after=%s,tolerance=%.3f",
                             DistanceChecker2::StateName(jumpStateBeforeCheck),
                             DistanceChecker2::StateName(s_distanceChecker2.CurrentState()), tolerance);
        FinalizeJumpDiag("recovering", "jump_reject", false);
#endif
        return true;
    }

#if MULTISTAR2_JUMP_DIAGNOSTICS
    // TODO(multistar2-jump-diagnostics): TEMPORARY
    jumpDiag.jumpState =
        wxString::Format("before=%s,check=accept,after=%s,tolerance=%.3f",
                         DistanceChecker2::StateName(jumpStateBeforeCheck),
                         DistanceChecker2::StateName(s_distanceChecker2.CurrentState()), tolerance);
#endif

    ImageLogger::LogImage(pImage, distance);
    UpdateCurrentDistance(distance, distanceRA);

    // Use a real star location for profile display.
    pFrame->pProfile->UpdateData(pImage, m_displayStar.X, m_displayStar.Y);
    pFrame->AdjustAutoExposure(m_displayStar.SNR);
    pFrame->UpdateStatusBarStarInfo(m_displayStar.SNR, m_displayStar.GetError() == Star::STAR_SATURATED);

    errorInfo->starError = Star::STAR_OK;
    errorInfo->starMass = m_displayStar.Mass;
    errorInfo->starSNR = m_displayStar.SNR;
    errorInfo->starHFD = m_displayStar.HFD;
    errorInfo->status = StarStatus2(m_displayStar);

    {
        bool primaryContrib = !m_starState.empty() && m_starState[0].contributingThisFrame;
        PHD_Point dDisp = disp - prevDisp;
        wxString usedIdxStr = JoinUnsigned(usedIdx);
        EmitFrameSummary("ok", "none", (unsigned int) found.size(), contributing, primaryContrib, distance, disp, dDisp, false,
                         usedIdxStr, addedStr, removedStr);
    }

#if MULTISTAR2_JUMP_DIAGNOSTICS
    // TODO(multistar2-jump-diagnostics): TEMPORARY
    FinalizeJumpDiag("ok", "ok", true);
#endif
    return false;
}

void GuiderMultiStar2::OnPaint(wxPaintEvent& event)
{
    wxAutoBufferedPaintDC dc(this);
    wxMemoryDC memDC;

    try
    {
        if (PaintHelper(dc, memDC))
            return;

        // Overlays:
        // - circles around contributing stars (green)
        // - circles around lost stars (orange dotted)
        // - box around aggregate solution point
        // - status text bottom-right: "Multistars: X/Y"
        // - "multistar2" tag top-right when selected

        EnsureStarStateSize();

        wxPen greenPen(wxColour(0, 255, 0), 1, wxPENSTYLE_SOLID);
        wxPen lostPen(wxColour(230, 130, 30), 1, wxPENSTYLE_DOT);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);

        // Match Star Profile "Mid row FWHM:" text color (red)
        const wxColour kOverlayTextColor(255, 0, 0);
        const wxColour prevTextColor = dc.GetTextForeground();
        dc.SetTextForeground(kOverlayTextColor);

        // Tag: show when the multistar2 guider has been selected/instantiated
        if (GetState() >= STATE_SELECTING)
        {
            wxString tag = wxS("multistar2");
            wxSize tsz = dc.GetTextExtent(tag);
            int x = XWinSize - tsz.GetWidth() - 5;
            int y = 5;
            dc.DrawText(tag, x, y);
        }

        // Draw circles for stars (skip if no multistar mode or subframes forced)
        bool useSubframes = pCamera && pCamera->UseSubframes;
        bool showStars = m_multiStarMode && m_guideStars.size() > 1 && !useSubframes && GetState() >= STATE_SELECTED;
        if (showStars)
        {
            for (size_t i = 0; i < m_guideStars.size(); i++)
            {
                const StarState& st = m_starState[i];
                if (!st.lastPosValid)
                    continue;

                wxPoint pt((int) (st.lastPos.X * m_scaleFactor), (int) (st.lastPos.Y * m_scaleFactor));
                if (st.contributingThisFrame)
                    dc.SetPen(greenPen);
                else
                    dc.SetPen(lostPen);
                dc.DrawCircle(pt, 6);
            }
        }

        // Draw box around aggregate solution point when star is selected+
        if (GetState() >= STATE_SELECTED && m_solutionStar.IsValid())
        {
            dc.SetPen(wxPen(wxColour(32, 196, 32), 1, wxPENSTYLE_SOLID));
            int side = (int) ROUND((m_searchRegion * 2 + 1) * m_scaleFactor);
            int left = (int) ROUND((m_solutionStar.X - m_searchRegion) * m_scaleFactor);
            int top = (int) ROUND((m_solutionStar.Y - m_searchRegion) * m_scaleFactor);

            dc.DrawRectangle(left, top, side, side);

            // UI: distinguish multistar2 by adding 45-degree corner ticks.
            // Make each tick as long as the side of the square.
            int tickLen = wxMax(1, side);
            int d = wxMax(1, (int) ROUND((double) tickLen / sqrt(2.0)));

            int x0 = left;
            int x1 = left + side;
            int y0 = top;
            int y1 = top + side;

            // top-left, top-right, bottom-left, bottom-right
            dc.DrawLine(x0, y0, x0 - d, y0 - d);
            dc.DrawLine(x1, y0, x1 + d, y0 - d);
            dc.DrawLine(x0, y1, x0 - d, y1 + d);
            dc.DrawLine(x1, y1, x1 + d, y1 + d);
        }

        // Status text: only when multistar2 is active and a star is selected
        if (GetState() >= STATE_SELECTED && m_multiStarMode)
        {
            wxString msg = wxString::Format(_("Multistars: %u/%u"), m_solutionStarsUsed, m_maxConcurrentStarsUsed);
            wxSize tsz = dc.GetTextExtent(msg);
            int x = XWinSize - tsz.GetWidth() - 5;
            int y = YWinSize - tsz.GetHeight() - 5;
            dc.DrawText(msg, x, y);
        }

        dc.SetTextForeground(prevTextColor);
    }
    catch (const wxString& Msg)
    {
        POSSIBLY_UNUSED(Msg);
    }
}

