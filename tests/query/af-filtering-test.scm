; Socrates, Einstein, and Peirce are all men.
(ConceptNode "Socrates" (stv .001 1) (cog-new-av 15 10 0))
(ConceptNode "Einstein" (stv .001 1) (cog-new-av 45 10 0))
(ConceptNode "Peirce" (stv .001 1) (cog-new-av 15 10 0))
(ConceptNode "man" (stv .01 1) (cog-new-av 35 10 0))

(InheritanceLink (stv 1.0 1.0)
    (ConceptNode "Socrates")
    (ConceptNode "man") )

(InheritanceLink (stv 1.0 1.0) 
    (ConceptNode "Einstein")
    (ConceptNode "man") (cog-new-av 35 10 0))

(InheritanceLink (stv 1.0 1.0)
    (ConceptNode "Peirce")
    (ConceptNode "man"))

; Pattern for the pattern matcher to match, to find them.
(define find-man
    (BindLink
        (VariableNode "$X")
        (InheritanceLink
            (VariableNode "$X")
            (ConceptNode "man"))
        (VariableNode "$X")))
