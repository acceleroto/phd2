/*
 *  guider_multistar2.h
 *  PHD Guiding
 *
 *  Experimental multi-star guider (opt-in via Advanced Settings).
 *
 *  Phase A: scaffold only. Behavior matches classic GuiderMultiStar.
 */
 
#ifndef GUIDER_MULTISTAR2_H_INCLUDED
#define GUIDER_MULTISTAR2_H_INCLUDED
 
#include "guider_multistar.h"
 
// Compile-time switch for extra multistar2 logging.
// Enable by adding -DMULTISTAR2_DEBUG_LOG=1 to your build.
#ifndef MULTISTAR2_DEBUG_LOG
#define MULTISTAR2_DEBUG_LOG 0
#endif

// Experimental multi-star guider. For Phase A this is a thin wrapper around the
// classic implementation; later phases will override selected virtual methods.
class GuiderMultiStar2 : public GuiderMultiStar
{
public:
    explicit GuiderMultiStar2(wxWindow *parent);
    ~GuiderMultiStar2() override;
 
    wxString GetSettingsSummary() const override;

    // Phase B/C: allow guiding to continue when the original primary star is lost
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

