# 8. Preuves : des affirmations aux exécutions reproductibles

Une affirmation étiquetée **DEVICE** ne vaut que par l'exécution qui la porte. Cette page relie chacune à un **dossier de preuve**
que toute personne ayant un 3GS peut régénérer, et dit clairement quelles affirmations n'en ont pas encore.

--8<-- "docs/snippets/build-commit.fr.md"

## Ce que contient un dossier

Chaque exécution de `tools/send.sh` ou `tools/go.sh` écrit un répertoire sous [`evidence/`](https://github.com/SebE585/trognon/tree/main/evidence) :

| Fichier | Ce qu'il prouve |
|---|---|
| `manifest.json` | le SHA-256 du payload, le commit git d'où il a été construit, si l'arbre était propre, le nonce envoyé et relu, le commit d'ipwndfu, la ligne de commande, le CPID de l'appareil (l'ECID n'est volontairement pas enregistré) |
| `screen.txt` | le texte et la couleur exacts de chaque ligne de console affichée par le téléphone, **publiés par le payload lui-même par USB** : ni photo ni OCR |
| `screen.png` | la console redessinée à partir de `screen.txt`. Une reconstruction, pas une photo du téléphone |
| `vars.txt` | les valeurs supplémentaires publiées par le payload (`--vars`) |

**Pourquoi le nonce compte.** L'iBSS garde ses variables d'un run à l'autre : une valeur périmée peut passer pour un résultat. Un nonce
aléatoire de 32 bits est ajouté au binaire, le payload le republie, et un dossier n'est écrit que s'il correspond. Les lignes d'écran
sont publiées en entier à chaque run (les vides aussi) pour la même raison.

## Dossiers

--8<-- "docs/snippets/evidence-table.fr.md"

Régénérer ce tableau avec `make evidence-index`. Rejouer les étapes avec `tools/evidence_all.sh` (appareil en DFU). Pour vérifier
un dossier : se placer sur son commit, `make -C tools/payloads <payload>`, comparer le SHA-256 avec `manifest.json` (la construction
est déterministe avec la même chaîne d'outils), puis lire `screen.txt` face à l'affirmation.

## Affirmations et leur dossier

| Affirmation (DEVICE) | Dossier | Statut |
|---|---|---|
| UNDEF et data abort pris et repris, `FSR`/`FAR`/`LR`/`SPSR` comme documenté | `trace` | avec dossier |
| Prefetch abort (`bkpt`), `IFSR = 2` | `trace_pabt` | avec dossier |
| IRQ logicielle de VIC0 prise une fois, activations restaurées | `trace_irq` | avec dossier |
| Valeurs des registres CLCD, GPIO et clock gates | `trace_regs` | avec dossier |
| État de VIC0/1/2 laissé par l'iBSS, mots de vecteur IRQ/FIQ | `trace_vic` | avec dossier |
| État du bloc timer, tick libre | `trace_timer` | avec dossier |
| Fréquence du tick à 24 MHz contre l'horloge du Mac | `tools/tick_rate.py` | **pas encore de dossier** : mesuré et consigné au journal seulement |
| Le code tourne depuis le DFU sans rien écrire dans le téléphone ; iBSS patché, `run` | le nonce de chaque dossier | avec dossier pour l'exécution ; les étapes DFU et pwn elles-mêmes non |
| La table de pages n'a aucune entrée de faute, toutes les sections en domaine 0, `DACR = 3` | payloads antérieurs | **sans dossier** : un run ancien, pas rejoué |
| Interruption du timer 5 | aucun | **non prouvée** (voir la [carte matérielle](03-hardware-map.md)) |
| Mapping GPIO des boutons, 376 broches lues, zéro bruit | `trace_buttons` | avec dossier |
| Identifiant NAND, CE0/CE1 seulement (Toshiba) | `trace_nand`, `trace_nand_sweep` x2 | avec dossier |
| GPIO reset/réveil Wi-Fi lues à `0` | `trace_wifi_gpio` | avec dossier |
| GPIO reset Wi-Fi basculée bas/haut, relue à `1` | `trace_wifi_reset` | avec dossier |
| Dump des registres SDIO, deux hypothèses de clock-gate, tout à zéro | `trace_sdio_dump`, `trace_sdio_dump_b` | avec dossier |
| Tentative `CMD0` SDIO, ne quitte jamais « pas prêt » | `trace_sdio_cmd0` | avec dossier |
| Séquence d'init SDIO façon `plat-s5l8900` héritée (pas SDHCI standard), toujours tout à zéro | `trace_sdio_init` | avec dossier |
| Test de persistance d'écriture du clock-gate (faussé, dépassé ci-dessous) | `trace_gate_write` | avec dossier |
| Écriture brute puis relecture sur la base SDIO, l'écriture ne tient pas | `trace_write_test` | avec dossier |
| Registre d'horloge dynamique 16, lecture pure, ambigu | `trace_dyn_clock16` | avec dossier |
| Timers 0-3, les 5 champs, rien ne change sur 0,5 s | `trace_which_timer`, `trace_which_timer_b` | avec dossier |

Une affirmation sans dossier vient d'un run antérieur et reste une piste tant qu'elle n'est pas rejouée.
