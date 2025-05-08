# The Three Pillars

> [!WARNING]
> This idea is still not _fully_ formed in my mind, so I might not
> articulate it especially well here.
> 
> I'll come back to this document as what I'm trying to say 
> continues to settle in my head.

![The Three Pillars Diagram of Simplicity, Security, and Speed](../images/diagrams/Three%20Pillars.png)

Anos is designed and built around three key pillars: **Simplicity**, **Security** and **Speed**.

These pillars, which are equally important, inform every design and 
implementation decision made when building Anos. The general idea
is:

* By aiming for **Simplicity**, we get **Security** by default...
* In targeting **Security**, we should never (unnecessarily) compromise **Speed**
* We will achieve **Speed** only if we strive to **Simplify**

The overall guiding principle is that, while different parts of the
system will naturally have different needs, we should always work 
to **Keep the circles the same size** as much as we possibly can,
while being pragmatic in our approach to balancing the sometimes-competing
goals across the pillars.
