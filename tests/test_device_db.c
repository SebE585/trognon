/* SPDX-License-Identifier: MIT */
/* Pure data tests, no hardware required. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "trognon/device.h"

int
main(void)
{
    assert(trognon_device_catalog_count > 0);

    const struct trognon_device *d3gs = trognon_device_find_by_identifier("iPhone2,1");
    assert(d3gs != NULL);
    assert(strcmp(d3gs->model_name, "iPhone 3GS") == 0);
    assert(d3gs->soc == TROGNON_SOC_S5L8920);
    assert(d3gs->unlock_path == TROGNON_UNLOCK_HOST_DIRECT);
    assert(d3gs->cpid == 0x8920);

    /* The lookup that actually works on a real device in Recovery/DFU:
     * by CPID, not by model_identifier (verified against a real iPhone
     * 3GS on 2026-10-14 — the literal model string never appears on the
     * bus). */
    assert(trognon_device_find_by_cpid(0x8920) == d3gs);
    assert(trognon_device_find_by_cpid(0) == NULL);
    assert(trognon_device_find_by_cpid(0xffff) == NULL);

    const struct trognon_device *d4 = trognon_device_find_by_identifier("iPhone3,1");
    assert(d4 != NULL);
    assert(strcmp(d4->model_name, "iPhone 4") == 0);
    assert(d4->soc == TROGNON_SOC_A4);
    assert(d4->unlock_path == TROGNON_UNLOCK_HOST_DIRECT); /* host-direct, no helper board (SHAtter on the A4) */

    const struct trognon_device *d4s = trognon_device_find_by_identifier("iPhone4,1");
    assert(d4s != NULL);
    assert(strcmp(d4s->model_name, "iPhone 4S") == 0);
    assert(d4s->soc == TROGNON_SOC_A5);
    assert(d4s->unlock_path == TROGNON_UNLOCK_HELPER_BOARD);
    assert(d4s->dt_platform == NULL); /* no mainline DT yet */

    const struct trognon_device *dx = trognon_device_find_by_identifier("iPhone10,3");
    assert(dx != NULL);
    assert(dx->soc == TROGNON_SOC_A11);
    assert(dx->support.storage == 1);

    /* Older siblings (research 2026-10-17): chip and exploit as read in ipwndfu's own dispatch. */
    const struct trognon_device *d2g = trognon_device_find_by_identifier("iPhone1,1");
    assert(d2g != NULL && d2g->soc == TROGNON_SOC_S5L8900 && d2g->cpid == 0x8900);
    assert(d2g->unlock_path == TROGNON_UNLOCK_UNKNOWN); /* ipwndfu has no CPID:8900 branch */
    assert(trognon_device_find_by_cpid(0x8900) == d2g); /* first entry of a CPID represents it */
    assert(trognon_device_find_by_identifier("iPhone1,2") != NULL);
    assert(trognon_device_find_by_identifier("iPod1,1") != NULL);

    const struct trognon_device *t2 = trognon_device_find_by_identifier("iPod2,1");
    assert(t2 != NULL && t2->soc == TROGNON_SOC_S5L8720 && t2->cpid == 0x8720);
    assert(t2->unlock_path == TROGNON_UNLOCK_HOST_DIRECT); /* steaks4uce */

    const struct trognon_device *t3 = trognon_device_find_by_identifier("iPod3,1");
    assert(t3 != NULL && t3->soc == TROGNON_SOC_S5L8922 && t3->cpid == 0x8922);
    assert(trognon_device_find_by_cpid(0x8922) == t3);

    const struct trognon_device *t4 = trognon_device_find_by_identifier("iPod4,1");
    assert(t4 != NULL && t4->soc == TROGNON_SOC_A4 && t4->cpid == 0x8930);
    assert(trognon_device_find_by_cpid(0x8930) == d4); /* the iPhone 4 stays the representative */

    assert(trognon_device_find_by_identifier("nonexistent") == NULL);
    assert(trognon_device_find_by_identifier(NULL) == NULL);

    /* Every catalog entry has non-null required string fields. */
    for (size_t i = 0; i < trognon_device_catalog_count; i++) {
        const struct trognon_device *d = &trognon_device_catalog[i];
        assert(d->model_name && d->model_identifier && d->soc_codename);
        assert(d->support.notes);
        assert(d->provenance_date && d->provenance_date[0]);
    }

    /* Provenance: only the iPhone 3GS has been confirmed by Trognon on a unit. A second
     * entry becoming DEVICE must be a deliberate edit of this test. */
    for (size_t i = 0; i < trognon_device_catalog_count; i++) {
        const struct trognon_device *d = &trognon_device_catalog[i];
        if (strcmp(d->model_identifier, "iPhone2,1") == 0)
            assert(d->provenance == TROGNON_PROV_DEVICE);
        else
            assert(d->provenance != TROGNON_PROV_DEVICE);
    }

    printf("test_device_db: %zu entries, all checks passed\n",
           trognon_device_catalog_count);
    return 0;
}
