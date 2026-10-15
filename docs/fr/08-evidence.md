# 8. Preuves : des affirmations aux exécutions reproductibles

Une affirmation étiquetée **DEVICE** ne vaut que par l'exécution qui la porte. Cette page relie chacune à un **dossier de preuve**
que toute personne ayant un 3GS peut régénérer, et dit clairement quelles affirmations n'en ont pas encore.

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

Une affirmation sans dossier vient d'un run antérieur et reste une piste tant qu'elle n'est pas rejouée.
