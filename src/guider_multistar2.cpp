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

GuiderMultiStar2::GuiderMultiStar2(wxWindow *parent) : GuiderMultiStar(parent)
{
    // Ensure our override is actually used for paint events. GuiderMultiStar registers its own
    // EVT_PAINT handler via an event table, which does not virtual-dispatch to overrides.
    Bind(wxEVT_PAINT, &GuiderMultiStar2::OnPaint, this);
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

        // Mark as "found but not contributing yet" if gated/rejected
        found.push_back({ i, s, eligible });
    }

    if (found.empty())
    {
        SetDroppedFrameInfo(pImage, errorInfo, _("Star lost"), 0.0, 0.0, 0.0, false, true);
        s_distanceChecker2.Activate();
        // Use max exposure duration while no usable stars are available.
        EmitFrameSummary("drop", "all_lost", (unsigned int) found.size(), 0, false, 0.0, prevDisp, PHD_Point(0.0, 0.0), false,
                         "", "", "");
        EmitRejectBreakdown("all_lost", (unsigned int) found.size(), 0, false, "", "", "");
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
            gs.referencePoint.X = f.star.X - baseDisp.X;
            gs.referencePoint.Y = f.star.Y - baseDisp.Y;
            gs.wasLost = false;

#if MULTISTAR2_DEBUG_LOG
            MS2LOGF("MultiStar2: reacquire idx=%u reacqGood=%u star=(%.2f,%.2f) baseDisp=(%.3f,%.3f) ref: (%.3f,%.3f)->(%.3f,%.3f)\n",
                    (unsigned int) f.idx, m_starState[f.idx].reacquireGoodCount, f.star.X, f.star.Y, baseDisp.X, baseDisp.Y,
                    oldRef.X, oldRef.Y, gs.referencePoint.X, gs.referencePoint.Y);
#endif
        }
        else if (!gs.referencePoint.IsValid())
        {
            gs.referencePoint.X = f.star.X - baseDisp.X;
            gs.referencePoint.Y = f.star.Y - baseDisp.Y;
            gs.wasLost = false;
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
        s_distanceChecker2.Activate();
        // Use max exposure duration while recovering from unusable contributors.
        EmitFrameSummary("recovering", "no_contributors", (unsigned int) found.size(), contributing, false, 0.0, prevDisp,
                         PHD_Point(0.0, 0.0), false, "", "", "");
        EmitRejectBreakdown("no_contributors", (unsigned int) found.size(), contributing, false, "", "", "");
        return true;
    }

    PHD_Point disp = baseDisp;
    if (sumW > 0.0)
        disp.SetXY(sumDX / sumW, sumDY / sumW);

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
        return true;
    }

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

