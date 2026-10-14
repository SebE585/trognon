/* SoC selection. One header per chip under soc/; exactly one is chosen with -DSOC_<NAME>.
 * Adding a chip = writing soc/<name>.h and re-verifying every constant on a device (docs/en/07-code-layout.md). */
#if defined(SOC_S5L8920)
# include "soc/s5l8920.h"
#elif defined(SOC_S5L8930)
# include "soc/s5l8930.h"
#else
# error "select a SoC, e.g. -DSOC_S5L8920 (make SOC=s5l8920)"
#endif
