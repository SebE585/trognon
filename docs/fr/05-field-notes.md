# 5. Notes de terrain : ce qui échoue et comment le voir

Le mode de défaillance dominant ici n'est pas le plantage, c'est la **réussite silencieuse** : le
système répond, mais avec l'ancien code ou une valeur périmée. Ces notes existent pour transformer
les échecs silencieux en échecs bruyants.

## Instabilité USB (DEVICE)

Environ 6 envois de payload sur 18 ont échoué en `USBError 5` (erreur d'E/S) ou `60` (délai
dépassé), puis l'appareil a montré le logo Apple ou est resté bloqué. Le même fichier est passé au
nouvel essai à chaque fois jusqu'ici. Un échec isolé ne dit donc rien du payload. Refaire un essai
depuis un DFU propre avant de soupçonner le code.

**L'essentiel venait d'un défaut de cache, corrigé (2026-10-15).** `run` saute à l'adresse du payload sans vider les caches du
processeur. Un payload envoyé après un autre à la même adresse pouvait exécuter les instructions et constantes du précédent,
et planter juste après `run` : presque un envoi sur deux d'une session échouait. Chaque payload commence maintenant par un prologue
identique octet pour octet (`hal/entry.c`) qui nettoie et invalide le cache de données sur la zone du payload et invalide le cache
d'instructions et le prédicteur de branchements. Six payloads à la suite ont ensuite tourné en 4 secondes, tous avec leur nonce
confirmé (voir [Preuves](08-evidence.md)). Un pwn raté, ou un envoi après une longue inactivité, peut toujours arriver.

## Marqueur périmé

L'iBSS garde la variable `trognon` d'un run à l'autre. Après un envoi raté, `getenv` renvoie la
valeur du run *précédent*. Toujours la lire avant le run (attendre `<unset>` après un DFU frais) et
regarder l'écran : un payload repeint tout l'affichage d'abord, donc une ancienne image à l'écran
signifie que le nouveau payload n'a pas tourné.

Les outils le vérifient maintenant à la place de l'œil : `tools/send.sh` ajoute un nonce aléatoire au
binaire, le payload le republie, et un marqueur périmé ne peut pas lui correspondre (voir la
[chaîne de démarrage](01-boot-chain.md)). Les payloads antérieurs au nonce sont signalés comme non confirmés.

## Échecs du pwn

`ipwndfu -p` annonce parfois `Exploit failed`. Le relancer, parfois après avoir remis en DFU,
fonctionne. Cela n'a pas abîmé l'appareil.

## Boucle batterie faussement morte (DEVICE)

Un réamorçage forcé (Accueil+Veille) sur une batterie usée peut faire disparaître l'icône
de batterie, puis la faire réapparaître en rouge, après une heure ou plus de charge — vu
sur un 3GS et un iPhone 4 jusqu'ici. Le cycle de l'icône donne l'impression que « ça charge,
il suffit d'attendre », mais c'est le symptôme d'une batterie usée, pas un problème de
durée de charge. Traiter le remplacement de batterie comme une étape de routine sur un
appareil d'occasion, pas comme un dernier recours de dépannage.

## Le CPID, pas l'étiquette, identifie la puce (DEVICE)

Le numéro de modèle ou l'annonce du vendeur d'un appareil d'occasion n'est pas fiable pour
distinguer les générations de puce (un « iPhone 4 » A4 et un « iPhone 4S » A5 se
ressemblent au premier coup d'œil, et les numéros de modèle se mélangent dans les
annonces). Le CPID lu par USB en mode DFU (`0x8920` pour ce 3GS, `0x8930` pour l'A4,
`0x8940` pour l'A5) est la seule source de vérité que ce projet utilise — voir la check-list
de réception, et la propre table d'aiguillage CPID → exploit d'`ipwndfu -p` (limera1n contre
SHAtter contre checkm8, [note de recherche](../research/2026-10-14-pre-a5-limera1n.md)).

## « Logo Apple » veut dire : recommencer

L'appareil a quitté la Recovery. Le payload RAM et l'état pwned ont disparu : DFU de nouveau.

## L'émulation est nécessaire et insuffisante

Les runs Unicorn hors ligne ont attrapé de vrais défauts avant l'envoi : GCC résolvait `&&label` au
début de la fonction (chaque exception gérée relançait tout le payload) ; un indicateur resté armé
après le premier test aurait ré-entré une adresse de reprise périmée. Elle ne voit pas ce qu'elle ne
modélise pas (`FSR`, `SCTLR.A`, caches, temps). Une émulation verte dit que le flux de contrôle est
bon, rien du matériel.

## Leçons diverses

- L'`env_set()` de l'iBSS ne préserve pas r4 à r11. Des variables locales gardées en registre à travers le
  premier appel sont revenues corrompues (un compteur lu à 0, un temps écoulé affiché d'une valeur à l'écran
  et d'une autre par USB). `hal/result.c` sauvegarde et restaure maintenant ces registres autour de l'appel.
  Quand un nombre publié contredit l'écran, soupçonner l'appel avant le matériel.
- La RAM scratch n'est pas remise à zéro entre payloads : initialiser chaque indicateur.
- Lire à l'œil sur un écran fait perdre des chiffres (on a lu `41000000C` pour `4100010C`). Garder
  un format d'affichage régulier et demander une relecture quand une valeur compte.
- Ne jamais recopier un chiffre de notre propre émulateur dans la doc comme une observation. Les
  faux `MIDR`/`SCTLR` de l'émulateur (`0x412FC0F1`, `0x00C50078`) ont été publiés comme valeurs DEVICE et
  n'ont été repérés qu'avec la photo de l'écran réel (`0x412FC081`, `0x00C5187D`). Étiqueter depuis
  la lecture de l'appareil, pas depuis un rendu.
- Une supposition fausse est documentée, pas effacée : elle fait partie du matériau.
