# Project Aet95

**"The fleet must grow, but you have no direct control"**

This project's goal is to make a programming game. The player is responsible of a fleet of machines in an alien planet.

They need to harvest resources, similarly to factory games. Raw materials are refined into other resources used to produce more advanced hardware and software. _Progression still need to be designed of course_

Each machine in the fleet will embed a aet-95 cpu following a simple but powerful ISA, limited resources, selected devices and a running program.

The player controls the design (combination of hardware and software). Examples (not all might make it, especially the duplication machine which might break a lot of things if not done carefully):

- A duplication machine -> more storage (physical and memory) and simple duplication program
- A networking machine -> high radio range, no motors, store data from other machines and broadcast information
- An explorer in early game -> low sensors, high ram, record map and useful information found

Player can poke devices via a MMIO. The fun of the game is the open-endedness of the design. Create specialized machines, find clever tricks. One interesting angle would be to bake intentional cpu bugs for the player to exploit. There would then be a chip revision system. This is seductive but very hard to design well.

The meta-progression will be unlocking better tech options. A compiler, an optimizer, a debugger, etc.. One apprehension I have is that the game would be too hard for beginners without a high-level programming language from the get-go. Historically, games like Turing complete and Zachtronics games have a really hard time attracting non-programmers. A problem that The Farmer was replaced does not have. It is fun for really skileld player to write assembly for a big chunk of the game. The better direction is to make it a hardmode (start with no compiler, first few hours are only aet95 assembly) and even honormode (reach a goal with never unlocking the compiler).

Early game machines might have a few Kib of ram and very low clock-speed.

## Physical environment

3D terrain. Mars-like for now, more exo-planet kind expansion angle if useful. I still have 2 legit directions open right now.

- Fixed terrain with resources being physical objects like Warcraft 3.
- Full voxel terrain. More expansion possibility but also a lot more work
