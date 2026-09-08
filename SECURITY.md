# Politique de sécurité

## Périmètre

a2adv est une chaîne d'outils locale : le compilateur (`a2c`, Python) tourne
sur votre poste à partir d'un fichier `.adv` que vous écrivez vous-même, le
player (C/6502) tourne sur un Apple II ou un émulateur sans réseau, et
l'éditeur (`editor/`) est une page web sans backend — rien n'est envoyé à un
serveur, rien n'est stocké ailleurs que dans le navigateur.

Une faille a un sens ici surtout si elle vient d'une **entrée non fiable** :
par exemple un fichier `.adv` ou un `STORY*.DAT` reçu de quelqu'un d'autre et
donné à `a2c` ou au player, ou une page/fichier importé dans l'éditeur.

## Versions couvertes

Pas de branches de version : seule la branche `main` est maintenue.

## Signaler une faille

Merci de **ne pas** ouvrir d'issue publique pour une faille de sécurité.
Utilisez l'onglet **Security → Report a vulnerability** de ce dépôt
([lien direct](https://github.com/bech-t/a2adv/security/advisories/new)) :
le rapport reste privé entre vous et le mainteneur jusqu'à correction.

Décrivez :
- le composant concerné (compilateur, player, éditeur) ;
- les étapes pour reproduire (idéalement un fichier `.adv` ou `.DAT` minimal) ;
- l'impact que vous pensez possible.

## À quoi s'attendre

C'est un projet à un seul mainteneur, sur son temps libre : pas de SLA, mais
un accusé de réception sous quelques jours et une correction publiée dès que
possible, avec crédit au rapporteur si souhaité.
