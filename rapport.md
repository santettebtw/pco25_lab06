# Rapport labo : Multiplication matricielle multithreadée par moniteurs

**Auteur :** Anthony Pfister, Santiago Sugranes  
**Date :** 19.12.2025  
**Cours :** PCO2025 lab06, HEIG

---

## 1. Contexte et choix de conception

Pour ce laboratoire, nous avons implémenté une multiplication matricielle multithreadée basée sur une décomposition en blocs.

Les threads sont créés une seule fois dans le constructeur de `ThreadedMatrixMultiplier` et restent actifs pendant toute la durée de vie de l’objet.

Le calcul est découpé en tâches correspondant aux triplets (i, j, k). Chaque tâche calcule une partie du résultat et est envoyée dans un buffer partagé.

Le buffer est implémenté comme un moniteur de Hoare. Il permet de synchroniser l’accès aux jobs et de suivre l’avancement des calculs.

Le thread principal ne fait aucun calcul directement et délègue tout le travail aux threads workers.

---

## 2. Analyse des problèmes de concurrence

L’accès concurrent au buffer de jobs est protégé par un moniteur de Hoare. Les threads attendent lorsqu’il n’y a pas de travail et sont réveillés lorsqu’un job est disponible.

La matrice résultat est partagée entre les threads. Les écritures sont sûres car chaque job ajoute uniquement une somme partielle sur des indices déterminés.

La fin d’un calcul est gérée par un compteur de jobs terminés. Le thread principal attend jusqu’à ce que tous les jobs soient complétés.

La terminaison des threads est gérée proprement dans le destructeur à l’aide d’un signal de terminaison.

---

## 3. Tests effectués

Les tests fournis ont été utilisés pour vérifier la correction du calcul et la réentrance de la méthode `multiply`.

- CHECK_DURATION : marche mais assez lent #TODO
- 


Tous les tests passent correctement et les résultats sont conformes aux attentes.

---
