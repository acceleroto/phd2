## Classic multistar vs multistar2: SNR weighting differences

### Where the SNR weighting happens in each guider

#### Classic multistar (`GuiderMultiStar::RefineOffset` in `src/guider_multistar.cpp`)
It starts with the **primary star offset** \(d_0 = (dx,dy)\) already computed from the primary star vs lock position:

- `sumX = origOffset.cameraOfs.X;`
- `sumY = origOffset.cameraOfs.Y;`
- `sumWeights = 1;`  (this is the primary’s weight)

Then for each **secondary** star that passes a bunch of gating, it computes that star’s displacement from its own `referencePoint`:

- \(d_i = (x_i - refX_i,\; y_i - refY_i)\)

and applies an SNR-derived weight:

- `wt = (pGS->SNR / m_primaryStar.SNR);`
- accumulates `sumX += wt * dX; sumY += wt * dY; sumWeights += wt;`
- then averages: `sumX /= sumWeights; sumY /= sumWeights;`

So the effective formula is:

\[
d_{\text{classic}} \;=\; \frac{1\cdot d_0 + \sum_i \left(\frac{\mathrm{SNR}_i}{\mathrm{SNR}_0}\right) d_i}{1 + \sum_i \left(\frac{\mathrm{SNR}_i}{\mathrm{SNR}_0}\right)}
\]

Multiply numerator+denominator by \(\mathrm{SNR}_0\):

\[
d_{\text{classic}} \;=\; \frac{\mathrm{SNR}_0 d_0 + \sum_i \mathrm{SNR}_i d_i}{\mathrm{SNR}_0 + \sum_i \mathrm{SNR}_i}
\]

That is: **a linear SNR-weighted mean including the primary**.

#### multistar2 (`GuiderMultiStar2::UpdateCurrentPosition` in `src/guider_multistar2.cpp`)
For each eligible star it uses:

- `w = (SNR > 0 ? SNR : 1.0)` (so it’s linear in SNR when SNR is positive)
- and computes the same kind of displacement from that star’s `referencePoint`:
  - \(d_i = (x_i - refX_i,\; y_i - refY_i)\)

Then:

\[
d_{\text{ms2}} \;=\; \frac{\sum_i w_i d_i}{\sum_i w_i}
\quad\text{with}\quad w_i=\mathrm{SNR}_i\ (\text{normally})
\]

### For the same set of stars, is the SNR weighting “the same”?
**Yes, if all SNR values are positive and the same stars are included**, the weighting is effectively the same:

- Classic uses weights proportional to \(\mathrm{SNR}_i\), just normalized by \(\mathrm{SNR}_0\). That normalization is a **constant scale factor** applied to all secondary weights, which **does not change** a weighted average.
- And if you rewrite classic’s formula (above), it’s literally the same as using **absolute SNR weights** including the primary.

So: **given identical included stars and their \(d_i\), both are linear-SNR weighted means**.

### When do they differ in practice?
They can differ because the *inputs* to the weighted average differ:

- **Classic sometimes ignores the weighted solution entirely**: after averaging, it only applies the multistar result if it’s *smaller* than the primary-only error (`hypot(avg) < primaryDistance`). If not, it falls back to the primary-only result. multistar2 does not have this “only if smaller” rule.
- **Different star inclusion/gating rules**:
  - Classic has stabilization logic and rejects secondaries based on excursions, miss counts, zero-count, max-star cap, etc.
  - multistar2 has its own gating (`eligible` based on reacquire-good-frames + massReject, etc.).
- **Corner case: non-positive SNR**:
  - Classic: \(wt = \mathrm{SNR}_i/\mathrm{SNR}_0\) → if a star’s SNR is 0, it contributes ~0 weight.
  - multistar2: if SNR ≤ 0, it uses weight 1.0 (so it would still contribute).
  - In real guiding, “found” stars typically have positive SNR, so this is usually theoretical.

### The biggest “behavioral” difference (not just weighting)
multistar2’s continuity mechanism (pinning `referencePoint` on reacquire / first-eligibility) is designed so **adding a star doesn’t change the aggregate displacement**, regardless of that star’s SNR weight. Classic doesn’t do that continuity pinning in the same way, so membership changes can shift the mean more readily.

