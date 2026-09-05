"""a2c — compilateur du DSL d'aventure .adv vers les données binaires Apple II.

Chaîne : source .adv  ->  parser  ->  table de symboles/validation  ->  encodeur
         ->  STORY.DAT + ASSETS.IDX  (cf. spec §6.1, §7ter).
"""

__version__ = "0.1.0"
