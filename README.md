# **Summary**

**Game Name: King of the Pirates**

**Game Concept:**  As a rising pirate, you and your crew are constantly being challenged by other pirates who also want to become the king. Defend yourselves and attack the pirates and try to knock them out of their boats to become the king of the pirates\!

# **Gameplay** {#gameplay}

Our game progresses turn by turn. The initial turn will have player 1 shoot their bullet and it will either hit or miss the target. If it hits the target, it will knock it back while also decreasing its hp. Then if there is player 2, it will be their turn to shoot. After both player 1 and player 2 have gone, as long as the target is not knocked out or out of hp, it will be the target’s turn to shoot. This process repeats until either the target or the player is knocked out or out of hp.

The win condition is to hit the opposing pirates enough to either deplete their hp bar or to knock them out of their ship. 

The loss condition is missing more than the opposing pirates such that all players are either knocked out of their ships or depleted of hp before the enemies.

There are different difficulty levels. 

1. Rookie mode: Low enemy accuracy & available aiming cursor  
2. Pirate King mode: High enemy accuracy & no aiming cursor

So the controls would be only through mouse cursors. From start to end for the player, it will take turns between the player and the enemy shooting each other until one side is out of hp or knocked out. As soon as this happens, the game stops and declares a win or a loss. For the graphics, we will make our own character models as well as the different projectiles. Our game will rely heavily on the physics engine as we will have to use the collision logic for when the projectile collides with the enemy entity, but also to calculate where the projectile is going to land after getting launched. 

# **Features** {#features}

**Priority 1 Features:**

- Collisions between bullet-character  
  - HP decreases when hit with a projectile, with the amount depending on projectile speed and base projectile damage  
- Collisions between character-water  
  - Characters die immediately if they fall of platforms into the ocean  
- Local multiplayer (switching turns based on which player went last)  
  - The turns should not switch to a dead character  
- Cursor Tracking  
  - Mouse button click  
  - Position relative to character to determine shooting power and velocity relative to platform character is on


**Priority 2 Features:**

- Enemy knockback (using physics engine) when hit with projectile  
- Enemy AI (can be random)  
  - Depending on difficulty, enemy AI have   
- Aiming system visualization  
  - Depending on level difficulty, an aiming visualization will be shown (sequence of dots)  
- Graphic Design (our own SVGs for player/enemy characters)

**Priority 3 Features:**

- Random level generation  
  - Platforms of different height  
  - Different number of enemies  
- Different Projectiles  
- Difficulty Selector  
  - Shown before starting the game. *Refer to [Gameplay](#gameplay) for more information*

**Priority 4 Features:**

- Sound effects  
- Music (changes when player 1, player 2 or enemy has low HP)  
- Backgrounds (dimmed when player about to die, changes depending on difficulty level)

