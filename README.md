## IRBaboon: Filter capture, transformation and convolution

Copyright © 2020 Felix Postma. All rights reserved.

##### Current version: 
Only available in debug mode in the JUCE AudioPluginHost. No steady release yet!

---

### What's the idea?

IRBaboon is a new funky tool that can quickly and easily capture, wonkify and apply audio filters, taken from real life or inside the computer. Now what does that mean?

Let's talk about sound like this: when you play a sound, it has to go from the source through several media, like the air and a speaker, to reach your ear (since brain chips don't quite exist yet). All the individual systems that the sound needs to go through can be thought of as **filters** that influence the sound going through it. So a filter could be a speaker, plain air, a whole room, your computer output or input, or a person standing in front of the speaker. And with enough care, these individual filters can be captured!

It is possible to capture the influence of any *linear, time-invariant* (LTI) filter. Essentially this means that capture is possible if the start time and the volume at which sound is played through it don't matter. If they do matter, then you can still capture it, it just won't sound the same as in real life.  
The way that the filter is captured is by recording an **impulse response** of the filter. This comprises the way the filter dampens, amplifies or delays the frequencies passing through it, and if LTI is observed then you will have a perfect reproduction of the real life system!

Impulse responses are often referred to as **IRs**, hence the name! They are the backbone of this idea, and are used all over, for example in convolution reverbs.  
The **Baboon** part refers to how this application is different: the digital domain allows us to flip IRs on their heads, make them more surreal than real and misbehave in other ways. So let's go and try that. 


### Projected features:
- Capture base and target configurations to isolate IRs
- Invert and wonkify captured or loaded IRs
- Standalone and VST versions for flexible application
