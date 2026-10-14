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

    const struct trognon_device *d4 = trognon_device_find_by_identifier("iPhone3,1");
    assert(d4 != NULL);
    assert(strcmp(d4->model_name, "iPhone 4") == 0);
    assert(d4->soc == TROGNON_SOC_A4);
    assert(d4->unlock_path == TROGNON_UNLOCK_HOST_DIRECT); /* limera1n, no helper board */

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

    assert(trognon_device_find_by_identifier("nonexistent") == NULL);
    assert(trognon_device_find_by_identifier(NULL) == NULL);

    /* Every catalog entry has non-null required string fields. */
    for (size_t i = 0; i < trognon_device_catalog_count; i++) {
        const struct trognon_device *d = &trognon_device_catalog[i];
        assert(d->model_name && d->model_identifier && d->soc_codename);
        assert(d->support.notes);
    }

    printf("test_device_db: %zu entries, all checks passed\n",
           trognon_device_catalog_count);
    return 0;
}
