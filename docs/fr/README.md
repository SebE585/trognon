# Documentation Trognon (français)

Guide de lecture du travail de rétro-ingénierie sur l'iPhone 3GS (S5L8920).
Chaque affirmation porte un niveau de preuve :

| Étiquette | Sens |
|---|---|
| **DEVICE** | Observé sur un vrai iPhone 3GS (lu à l'écran ou par USB). |
| **EMU** | Vu seulement dans notre émulation hors ligne (Unicorn). Prouve la logique, pas le matériel. |
| **SOURCE** | Lu dans OpenIBoot ou dans un désassemblage, pas encore vérifié sur l'appareil. |
| **HYPOTHESIS** | Un plan ou une supposition. Ne rien construire dessus sans vérifier. |

État au 2026-10-15, un seul appareil (ECID masqué), iBSS 4.3.5 pour `n88ap`
(CPID 0x8920). Tout ce qui a plus de six mois se re-vérifie.

1. [Chaîne de démarrage : du DFU à notre code](01-boot-chain.md)
2. [La console d'exceptions à l'écran](02-exception-console.md)
3. [Carte matérielle : ce qui est vérifié](03-hardware-map.md)
4. [Mods : construire l'OS un cas d'usage à la fois](04-mods.md)
5. [Notes de terrain : ce qui échoue et comment le voir](05-field-notes.md)
6. [Support par puce : 3GS, 4, 4S, puis le reste](06-chips.md)
7. [Code modulaire](07-code-layout.md)
8. [Preuves : des affirmations aux exécutions reproductibles](08-evidence.md)

English: [`../en/README.md`](../en/README.md).
Anciennes notes de travail (mêlées au journal) : [`../architecture.md`](../architecture.md).

Aucun firmware Apple n'est stocké dans ce dépôt. L'iBSS utilisé ici est déchiffré sur
l'appareil du propriétaire et patché en RAM.

## Provenance et licences

Les adresses de registres et les faits de démarrage ont été *lus* dans OpenIBoot (GPL-3.0,
inactif depuis 2022) puis vérifiés sur l'appareil. Trognon ne copie pas le code d'OpenIBoot :
nos payloads sont écrits de zéro, seuls les adresses et décalages (des faits) sont réutilisés,
chacun marqué **SOURCE** tant qu'il n'est pas lu sur le téléphone. La GPL-3.0 d'OpenIBoot est
incompatible avec les composants GPL-2.0-only et MIT du projet : aucun code d'OpenIBoot ne doit
être collé ici.
