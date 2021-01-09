## IRBaboon: Filter capture, transformation and convolution

This project is meant for doing applied exploration and experimentation into convolution and impulse response capture in VST form. The work of [Farina](http://www.melaudia.net/zdoc/sweepSine.PDF) regarding exponential sine sweeps and the work of [Battenberg and Aviûienis](https://www.semanticscholar.org/paper/IMPLEMENTING-REAL-TIME-PARTITIONED-CONVOLUTION-ON-Battenberg-Avi%C3%BBienis/2e4c2797267dd50850eb38fd9d33b9ad7f8d407c?p2df) regarding uniformly partitioned convolution has been directly implemented. It uses the JUCE framework and its FFTs, but the rest of the DSP code is handwritten. 

The original goal was to perfectly compensate for an LTI speaker obstruction, test cases being a pillow and a resonant pipe in front of a speaker. This goal was reached, and the project can be turned into a pretty useful and fun VST!

Sadly there is no stable release yet. Stay tuned!

---

### What's the idea?

IRBaboon is a quirky VST tool that can quickly and easily capture, wonkify and apply audio filters, taken from real life or inside the computer. Now what does that mean?

When you play a sound, it has to go from the source through several media, like the air and a speaker, to reach your ear (since brain chips don't quite exist yet). All the individual systems that the sound needs to go through can be thought of as **filters** that influence the sound going through it. So a filter could be a speaker, plain air, a whole room, your computer output or input, or a person standing in front of the speaker. And with enough care, these individual filters can be captured!

It is possible to capture the influence of any *linear, time-invariant* (LTI) filter. Essentially this means that capture is possible if the start time and the volume at which sound is played through it don't matter. If they do matter, then you can still capture it, it just won't sound the same as in real life.  
The way that the filter is captured is by recording an **impulse response** of the filter. This comprises the way the filter dampens, amplifies or delays the frequencies passing through it, and if LTI is observed then you will have a perfect reproduction of the real life system!

Impulse responses are often referred to as **IRs**, hence the name! They are the backbone of this idea, and are used all over, for example in convolution reverbs.  
The **Baboon** part refers to how this application is different: the digital domain allows us to flip IRs on their heads, make them more surreal than real and misbehave in other ways. 


### Projected features:
- Capture base and target configurations to isolate IRs
- Invert and wonkify captured or loaded IRs
- Standalone and VST versions for flexible application
