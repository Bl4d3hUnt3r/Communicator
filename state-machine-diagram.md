          +---------------+
          |  State 0    |
          |  (Initialization) |
          +---------------+
                  |
                  |  A presses button
                  v
          +---------------+
          |  State 4    |
          |  (Wake-up)    |
          +---------------+
                  |
                  |  B receives wake-up
                  v
          +---------------+
          |  State 5    |
          |  (Interacting) |
          +---------------+
                  |
                  |  User sets state
                  |  and presses send
                  v
          +---------------+
          |  State 1-3  |
          |  (Desired state) |
          +---------------+
                  |
                  |  Timeout (30 sec)
                  v
          +---------------+
          |  State 0    |
          |  (Reset)     |
          +---------------+
