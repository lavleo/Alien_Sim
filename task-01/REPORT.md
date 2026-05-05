
#Overview

The project I did was based of the film Alien (and its subsequent films). The film takes place on a cargo spaceship and the crew comes across an alien egg. The environment in the simulation is supposed to represent the closed in environment the characters were in. For most of the film, the crew are too far away from earth and are just trying to hide and survive from the Xenomorph. For most of my simulation, the prey are just running around and when the predator comes near, they run away. But once some time has passed, the prey run to the "shuttle" to leave the space ship. They prey are slower because they are carrying oxygen tanks as well just like in the original film.Similarly, as the Xenomorph eats more people, its grows bigger, stronger and faster. As the predator in the simulation eats more prey, it gets faster and faster.

##Challengees

1. The first big problem was the config file not being read. I had worked on all the other files and it should've ran, but the dashboard would not play anything. I still worked on the files, but blindly and after asking Proffessor for help, I was able to run the simulation without a hitch.
2. A problem occured when I was added the feature of the predator getting faster. I originally had its speed go up each time it ate, but once it got to a certain speed, it would get stuck in place chasing a prey. The predator also went way to fast for it to be realistic. So I had to find an ammount that worked well for the simulation. That number was around 75 prey which allowed for for most of the simulation, the predator wasn't killing the prey too easily.
3.Another problem was how to implement the shuttle. I didn't want it to be available for most of the time because too many prey would escape and it would not be faithful to the film. ALso for some reason, if the shuttle is too small, the prey do not path to it and get removed. So I had to make the shuttle bigger. ALso, I made it so the prey run around trying to escape the prey for 1000 seconds but after the path to the shuttle to leave.

### Results

The simulation I created was pretty accurate. The Predator stays in the center in the beggining, not being able to catch any prey just like the film. But once it "hatches", it catches the only prey near it.After that, it searches the area for nearby prey and chases after them. The prey do not try to run away when its far but when it gets near, they run away. They are able to run away at the beginning when the predator is still pretty slow. But once the predator catches more prey and gets faster, even they are too slow to run away efficiently. But the prey are not just running away from the whole time and doomed. After a point in time, the shuttle is able to use and the prey track to it while still evaded the already fast predator. Once they get to the shuttle, they are able to escape the Prey.

###Improvements

1. Having an actual ship with hallways and rooms would be ideal. But with time constraints and efficiency, I could not think of a good way to have an actual spaceship as the environment. 
2. In some zones, I would have liked the prey to be completly oblivious to the Preadtor like in the film. I decided against it because I wasn't able to implement the spaceship environment.
3. Also, I couldn't think of an idea for the prey to do before they get on the shuttle. In the film, they kind of just try not to be eaten, the simulation could have the prey doing jobs around the ship or something like that.