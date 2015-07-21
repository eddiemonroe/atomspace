;
; utilities.scm
;
;;; Commentary:
;
; Miscellaneous handy utilities for working with atoms.
; The most useful utilities in here are probably 'cog-chase-link'
; for finding atoms connected by a given link type, and 'cog-get-pred'
; which is useful for working with EvaluationLink's.
;
; Utilities include:
; -- abbreviations for working with truth values (stv, ctv, etc.)
; -- simple traversal of outgoing set (gar, gdr, etc.)
; -- for-each-except loop.
; -- cog-atom-incr --  Increment count truth value on "atom" by "cnt".
; -- purge-hypergraph -- purge a hypergraph and everything "under" it.
; -- purge-type -- purge all atoms of type 'atom-type'.
; -- clear -- purge all atoms in the atomspace.
; -- cog-get-atoms -- Return a list of all atoms of type 'atom-type'
; -- cog-prt-atomspace -- Prints all atoms in the atomspace
; -- cog-count-atoms -- Count of the number of atoms of given type.
; -- cog-report-counts -- Return an association list of counts.
; -- cog-get-root -- Return all hypergraph roots containing 'atom'
; -- cog-get-all-nodes -- Get all the nodes within a link and its sublinks
; -- cog-get-partner -- Return other atom of a link connecting two atoms.
; -- cog-pred-get-partner -- Get the partner in an EvaluationLink.
; -- cog-filter -- return a list of atoms of given type.
; -- cog-chase-link -- Return other atom of a link connecting two atoms.
; -- cog-chase-link-chk -- chase a link, with checking
; -- cog-map-chase-link -- Invoke proc on atoms connected through type.
; -- cog-par-chase-link -- call proc on atom connected via type. (parallel)
; -- cog-map-chase-links -- Invoke proc on atoms connected through type.
; -- cog-par-chase-links -- call proc on atoms connected via type. (parallel)
; -- cog-map-chase-links-chk -- Invoke proc on atom connected through type.
; -- cog-par-chase-links-chk -- call proc on atoms connected via type. (pllel)
; -- cog-map-chase-link-dbg -- Debugging version of above.
; -- cog-map-apply-link -- call proc on link between atom and atom type.
; -- cog-get-link -- Get list of links connecting atom to atom type.
; -- cog-get-pred -- Find all EvaluationLinks of given form.
; -- filter-hypergraph -- recursively traverse outgoing links of graph.
; -- cartesian-prod -- create Cartesian product from tuple of sets.
; -- cartesian-prod-list-only -- Alternative version of cartesian-prod.
; -- approx-eq? -- Test equality of 2 floats up to an epsilon
; -- cog-equal? -- Test equality of 2 atoms and returns TRUE_TV/FALSE_TV
; -- max-element-by-key -- Get maximum element in a list
; -- min-element-by-key -- Get maximum element in a list
;
;;; Code:
; Copyright (c) 2008, 2013, 2014 Linas Vepstas <linasvepstas@gmail.com>
;

(use-modules (srfi srfi-1))
(use-modules (ice-9 threads))  ; needed for par-map par-for-each
; (use-modules (system base compile)) ;; needed for compiler

(define (av sti lti vlti) (cog-new-av sti lti vlti))

(define (stv mean conf) (cog-new-stv mean conf))
(define (itv lower upper conf) (cog-new-itv lower upper conf))
(define (ctv mean conf count) (cog-new-ctv mean conf count))
(define (ptv mean conf count) (cog-new-ptv mean conf count))
(define (ftv mean conf) (cog-new-ftv mean conf))

; Fetch the mean, confidence and count of a TV.
(define (tv-mean tv)
"
  Return the floating-point mean (strength) of a TruthValue.
"
	(assoc-ref (cog-tv->alist tv) 'mean))

(define (tv-conf tv)
"
  Return the floating-point confidence of a TruthValue.
"
	(assoc-ref (cog-tv->alist tv) 'confidence))
;
; Simple truth values won't have a count. Its faster to just check
; for #f than to call (cog-ctv? tv)
(define (tv-count tv)
"
  Return the floating-point count of a CountTruthValue.
"
	(define cnt (assoc-ref (cog-tv->alist tv) 'count))
	(if (eq? cnt #f) 0 cnt)
)

; -----------------------------------------------------------------------
; analogs of car, cdr, etc. but for atoms.
; (define (gar x) (if (cog-atom? x) (car (cog-outgoing-set x)) (car x)))
; (define (gdr x) (if (cog-atom? x) (cadr (cog-outgoing-set x)) (cdr x)))

(define (gar x) (car (cog-outgoing-set x)) )
(define (gdr x) (cadr (cog-outgoing-set x)) )
(define (gadr x) (gar (gdr x)) )
(define (gddr x) (gdr (gdr x)) )
(define (gaddr x) (gar (gddr x)) )
(define (gdddr x) (gdr (gddr x)) )

; A more agressive way of doing the above:
; (define car (let ((oldcar car)) (lambda (x) (if (cog-atom? x) (oldcar (cog-outgoing-set x)) (oldcar x)))))
; But this would probably lead to various painful debugging situations.

; -----------------------------------------------------------------------
; for-each-except
(define (for-each-except exclude proc lst)
"
  Standard for-each loop, except that anything matching 'except' is skipped
"
	(define (loop items)
		(cond
			((null? items) #f)
			((eq? exclude (car items))
				(loop (cdr items))
			)
			(else
				(proc (car items))
				(loop (cdr items))
			)
		)
	)
	(loop lst)
)

; --------------------------------------------------------------
;
(define (cog-atom-incr atom cnt)
"
  cog-atom-incr --  Increment count truth value on 'atom' by 'cnt'

  If the current truth value on the atom is not a CountTruthValue,
  then the truth value is replaced by a CountTruthValue, with the
  count set to 'cnt'.  The mean and confidence values are left
  untouched.

  XXX this implementation is slow/wasteful, a native C++ would
  be considerably faster.

  Example usage:
  (cog-atom-incr (ConceptNode 'Answer') 42)
"
	(let* (
			(tv (cog-tv atom))
			(atv (cog-tv->alist tv))
			(mean (assoc-ref atv 'mean))
			(conf (assoc-ref atv 'confidence))
			(count (assoc-ref atv 'count))

			; non-CountTV's will not have a 'count in the a-list
			; so its enough to test for that.
			(ntv (if count
					(cog-new-ctv mean conf (+ count cnt))
					(cog-new-ctv mean conf cnt))
			)
		)
		(cog-set-tv! atom ntv)
	)
)

; --------------------------------------------------------------------
(define (purge-hypergraph atom)
"
  purge-hypergraph -- purge a hypergraph and everything under it

  If the indicated atom has no incoming links, then purge it. Repeat
  recursively downwards, following the *outgoing* set of any links
  encountered.  This only removes the atoms from the atomspace, it
  does NOT remove it from the backingstore, if attached!
"
	(if (cog-node? atom)
		(cog-purge atom)
		(let* ((oset (cog-outgoing-set atom))
				(flg (cog-purge atom))
			)
			(if flg ;; halt recursion if link was not purge-able
				(for-each purge-hypergraph oset)
			)
		)
	)
)

; --------------------------------------------------------------------
(define (purge-type atom-type)
"
  purge-type -- purge all atoms of type 'atom-type'

  If any atoms of that type have incoming links, those links will be
  purged, and so on recursively.  This only removes the atoms from the
  atomspace, it does NOT remove it from the backingstore, if attached!
"
	(cog-map-type
		(lambda (x) (cog-purge-recursive x) #f)
		atom-type
	)
)

; --------------------------------------------------------------------
(define (clear)
"
  clear -- purge all atoms in the atomspace.  This only removes the
  atoms from the atomspace, it does NOT remove it from the backingstore,
  if attached!
"
	(for-each
		purge-type
		(cog-get-types)
	)
)

(define (count-all)
	(define cnt 0)
	(define (ink a) (set! cnt (+ cnt 1)) #f)
	(define (cnt-type x) (cog-map-type ink x))
	(map cnt-type (cog-get-types))
	cnt
)

; -----------------------------------------------------------------------
(define (cog-get-atoms atom-type)
"
  cog-get-atoms -- Return a list of all atoms of type 'atom-type'

  cog-get-atoms atom-type
  Return a list of all atoms in the atomspace that are of type 'atom-type'

  Example usage:
  (display (cog-get-atoms 'ConceptNode))
  will return and display all atoms of type 'ConceptNode
"
	(let ((lst '()))
		(define (mklist atom)
			(set! lst (cons atom lst))
			#f
		)
		(cog-map-type mklist atom-type)
		lst
	)
)

; -----------------------------------------------------------------------
(define (cog-prt-atomspace)
"
  cog-prt-atomspace -- Prints all atoms in the atomspace

  This will print all of the atoms in the atomspace: specifically, only those
  atoms that have no incoming set, and thus are at the top of a hierarchy.
  All other atoms (those which do have an incoming set) then appear somewhere
  underneath these top-most atoms.

  Example usage:
  (display (cog-get-atoms 'ConceptNode))
  will return and display all atoms of type 'ConceptNode
"
	(define (prt-atom h)
		; print only the top-level atoms.
		(if (null? (cog-incoming-set h))
			(display h))
	#f)
	(define (prt-type type)
		(cog-map-type prt-atom type)
		; We have to recurse over sub-types
		(for-each prt-type (cog-get-subtypes type))
	)
	(prt-type 'Atom)
)

; -----------------------------------------------------------------------
(define (cog-count-atoms atom-type)
"
  cog-count-atoms -- Count of the number of atoms of given type

  cog-count-atoms atom-type
  Return a count of the number of atoms of the given type 'atom-type'

  Example usage:
  (display (cog-count-atoms 'ConceptNode))
  will display a count of all atoms of type 'ConceptNode
"
	(let ((cnt 0))
		(define (inc atom)
			(set! cnt (+ cnt 1))
			#f
		)
		(cog-map-type inc atom-type)
		cnt
	)
)

; -----------------------------------------------------------------------
(define (cog-report-counts)
"
  cog-report-counts -- Return an association list of counts

  Return an association list holding a report of the number of atoms
  of each type currently in the atomspace. Counts are included only
  for types with non-zero atom counts.
"
	(let ((tlist (cog-get-types)))
		(define (rpt type)
			(let ((cnt (cog-count-atoms type)))
				(if (not (= 0 cnt))
					(cons type cnt)
					#f
				)
			)
		)
		(filter-map rpt tlist)
	)
)

; -----------------------------------------------------------------------
(define (cog-get-root atom)
"
  cog-get-root -- Return all hypergraph roots containing 'atom'

  Return the root links of a specific 'atom'; basically get the root link
  with no incoming set.
"
	(define iset (cog-incoming-set atom))
	(if (null? iset)
		(list atom)
		(append-map cog-get-root iset))
)

; -----------------------------------------------------------------------
(define (cog-get-all-nodes link)
"
  cog-get-all-nodes -- Get all the nodes within a link and its sublinks

  Get all the nodes (non-link atoms) within a hypergraph, and return as
  a list.
"
	(define oset (cog-outgoing-set link))
	(define (recursive-helper atom)
		(if (cog-link? atom)
			(cog-get-all-nodes atom)
			(list atom)
		)
	)

	(append-map recursive-helper oset)
)

; -----------------------------------------------------------------------
(define (cog-get-partner pare atom)
"
  cog-get-partner -- Return other atom of a link connecting two atoms
  cog-get-partner pair atom

  If 'pare' is a link containing two atoms, and 'atom' is one of the
  two atoms, then this returns the other atom in the link.

  See also cog-chase-link which does not require the link to be
  explicitly specified; instead, only the pare type is needed.
"
	(let ((plist (cog-outgoing-set pare)))
		(if (equal? atom (car plist))
			(cadr plist)
			(car plist)
		)
	)
)

; -----------------------------------------------------------------------
(define (cog-pred-get-partner rel atom)
"
  cog-pred-get-partner -- Get the partner in an EvaluationLink

  cog-pred-get-partner pred atom

  Get the partner to the atom 'atom' in the opencog predicate 'pred'.
  An opencog predicate is assumed to be structured as follows:

     EvaluationLink
         SomeAtom  \"relation-name\"
         ListLink
             AnotherAtom  \"some atom\"
             AnotherAtom  \"some other atom\"

  Assuming this structure, then, given the top-level link, and one
  of the two atoms in the ListLink, this routine returns the other
  atom in the listLink.
"
	; The 'car' appears here because 'cog-filter' is returning
	; a list, and we want just one atom (the only one in the list)
	(cog-get-partner (car (cog-filter 'ListLink (cog-outgoing-set rel))) atom)
)

; -----------------------------------------------------------------------
(define (cog-filter atom-type atom-list)
"
  cog-filter -- return a list of atoms of given type.

  Given a list of atoms, return a list of atoms that are of 'atom-type'
"
	(define (is-type? atom) (eq? atom-type (cog-type atom)))
	(filter is-type? atom-list)
)

; -----------------------------------------------------------------------
(define (cog-chase-link link-type endpoint-type anchor)
"
  cog-chase-link -- Return other atom of a link connecting two atoms.

  cog-chase-link link-type endpoint-type anchor

  Starting at the atom 'anchor', chase its incoming links of
  'link-type', and return a list of all of the atoms of type
  'endpoint-type' in those links. For example, if 'anchor' is the
  node 'GivenNode \"a\"', and the atomspace contains

     SomeLink
         GivenNode \"a\"
         WantedNode  \"p\"

     SomeLink
         GivenNode \"a\"
         WantedNode  \"q\"

  then this method will return the two WantedNodes's, given the
  GivenNode as anchor, and the link-type 'SomeLink.

  viz: (cog-chase-link 'SomeLink 'WantedNode (GivenNode \"a\")) will
  return ((WantedNode \"p\") (WantedNode \"q\"))

  It is presumed that 'anchor' points to some atom (typically a node),
  and that it has many links in its incoming set. So, loop over all of
  the links of 'link-type' in this set. They presumably link to all
  sorts of things. Find all of the things that are of 'endpoint-type'.
  Return a list of all of these.

  See also: cgw-follow-link, which does the same thing, but for wires.
"
	(let ((lst '()))
		(define (mklist inst)
			(set! lst (cons inst lst))
			#f
		)
		(cog-map-chase-link link-type endpoint-type mklist anchor)
		lst
	)
)

; -----------------------------------------------------------------------
(define (cog-chase-link-chk link-type endpoint-type anchor anchor-type)
"
  cog-chase-link-chk -- chase a link with checking

  cog-chase-link link-type endpoint-type anchor anchor-type

  Same as above, but throws an error if anchor is not an atom of
  'anchor-type'.
"
	(let ((lst '()))
		(define (mklist inst)
			(set! lst (cons inst lst))
			#f
		)
		(cog-map-chase-links-chk link-type endpoint-type mklist anchor anchor-type)
		lst
	)
)

; -----------------------------------------------------------------------
(define (cog-map-chase-link link-type endpoint-type proc anchor)
"
  cog-map-chase-link -- Invoke proc on atom connected through type.

  Similar to cog-chase-link, but invokes 'proc' on the wanted atom.
  Starting at the atom 'anchor', chase its incoming links of
  'link-type', and call proceedure 'proc' on all of the atoms of
  type 'node-type' in those links. For example, if 'anchor' is the
  node 'GivenNode \"a\"', and the atomspace contains

     SomeLink
         GivenNode \"a\"
         WantedNode  \"p\"

     SomeLink
         GivenNode \"a\"
         WantedNode  \"q\"

  then 'proc' will be called twice, with each of the WantedNodes's
  as the argument. These wanted nodes were found by following the
  link type 'SomeLink, starting at the anchor GivenNode \"a\".

  It is presumed that 'anchor' points to some atom (typically a node),
  and that it has many links in its incoming set. So, loop over all of
  the links of 'link-type' in this set. They presumably link to all
  sorts of things. Find all of the things that are of 'endpoint-type'.
  Apply proc to each of these.
"
	(define (get-endpoint w)
		(map proc (cog-filter endpoint-type (cog-outgoing-set w)))
	)

	; We assume that anchor is a single atom, or empty list...
	(if (null? anchor)
		'()
		(map get-endpoint (cog-filter link-type (cog-incoming-set anchor)))
	)
)

(define (cog-par-chase-link link-type endpoint-type proc anchor)
"
  cog-par-chase-link -- call proc on atom connected via type. (parallel)

  Same as cog-map-chase-link, but a multi-threaded version: the work is
  distributed over the available CPU's.
"
	(define (get-endpoint w)
		(map proc (cog-filter endpoint-type (cog-outgoing-set w)))
	)

	; We assume that anchor is a single atom, or empty list...
	(if (null? anchor)
		'()
		(par-map get-endpoint (cog-filter link-type (cog-incoming-set anchor)))
	)
)

; -----------------------------------------------------------------------
(define (cog-map-chase-links link-type endpoint-type proc anchor)
"
  cog-map-chase-links -- Invoke proc on atom connected through type.

  Same as cog-chase-link, except that the anchor may be a single atom,
  or it may be a list.
"
	(if (list? anchor)
		(map
			(lambda (one-of)
				(cog-map-chase-links link-type endpoint-type proc one-of)
			)
		anchor)
		(cog-map-chase-link link-type endpoint-type proc anchor)
	)
)

(define (cog-par-chase-links link-type endpoint-type proc anchor)
"
  cog-par-chase-links -- call proc on atoms connected via type. (parallel)

  Same as cog-map-chase-links, but a multi-threaded version: the work
  is distributed over the available CPU's.  If anchor is a list, its
  still walked serially; the parallelism is in the incoming set of the
  anchor.  (which makes sense, since the incoming set is most likely to
  be manifold).
"
	(if (list? anchor)
		(map
			(lambda (one-of)
				(cog-par-chase-links link-type endpoint-type proc one-of)
			)
		anchor)
		(cog-par-chase-link link-type endpoint-type proc anchor)
	)
)

; -----------------------------------------------------------------------
(define (cog-map-chase-links-chk link-type endpoint-type proc anchor anchor-type)
"
  cog-map-chase-links-chk -- Invoke proc on atom connected through type.

  Same as cog-chase-links, except that the type of the anchor is
  checked, and an exception thrown if its the wrong type.
"
	(if (list? anchor)
		; If we are here, then anchor is a list. Recurse.
		(map
			(lambda (one-of)
				(cog-map-chase-links-chk link-type endpoint-type proc one-of anchor-type)
			)
		anchor)
		; If we are here, then its a singleton. Verify type.
		(if (eq? (cog-type anchor) anchor-type)
			(cog-map-chase-link link-type endpoint-type proc anchor)
			(throw 'wrong-atom-type 'cog-map-chase-links
				"Error: expecting atom:" anchor)
		)
	)
)

(define (cog-par-chase-links-chk link-type endpoint-type proc anchor anchor-type)
"
  cog-par-chase-links-chk -- call proc on atoms connected via type. (parallel)

  Same as cog-map-chase-links-chk, but a multi-threaded version: the work
  is distributed over the available CPU's.
"
	(if (list? anchor)
		; If we are here, then anchor is a list. Recurse.
		(map
			(lambda (one-of)
				(cog-par-chase-links-chk link-type endpoint-type proc one-of anchor-type)
			)
		anchor)
		; If we are here, then its a singleton. Verify type.
		(if (eq? (cog-type anchor) anchor-type)
			(cog-par-chase-link link-type endpoint-type proc anchor)
			(throw 'wrong-atom-type 'cog-map-chase-links
				"Error: expecting atom:" anchor)
		)
	)
)

; -----------------------------------------------------------------------
(define (cog-map-chase-link-dbg link-type endpoint-type dbg-lmsg dbg-emsg proc anchor)
"
  cog-map-chase-link-dbg -- debugging version of cog-map-chase-link

  cog-map-chase-link-dbg link-type endpoint-type dbg-lmsg dbg-emsg proc anchor

  Chase 'link-type' to 'endpoint-type' and apply proc to what is found there.

  It is presumed that 'anchor' points to some atom (typically a node),
  and that it has many links in its incoming set. So, loop over all of
  the links of 'link-type' in this set. They presumably link to all
  sorts of things. Find all of the things that are of 'endpoint-type'
  and then call 'proc' on each of these endpoints. Optionally, print
  some debugging msgs.

  The link-chasing halts if proc returns any value other than #f.
  Returns the last value returned by proc, i.e. #f, or the value that
  halted the iteration.

  Example usage:
  (cog-map-chase-link-dbg 'ReferenceLink 'WordNode '() '() proc word-inst)
  Given a 'word-inst', this will chase all ReferenceLink's to all
  WordNode's, and then will call 'proc' on these WordNodes.
"
	(define (get-endpoint w)
		(if (not (eq? '() dbg-emsg)) (display dbg-emsg))
		(for-each proc (cog-filter endpoint-type (cog-outgoing-set w)))
	)
	(if (not (eq? '() dbg-lmsg)) (display dbg-lmsg))
	(if (null? anchor)
		'()
		(for-each get-endpoint (cog-filter link-type (cog-incoming-set anchor)))
	)
)

; -----------------------------------------------------------------------
;
(define (cog-map-apply-link link-type endpoint-type proc anchor)
"
  cog-map-apply-link link-type endpoint-type proc anchor

  Similar to cog-map-chase-link, except that the proc is not called
  on the endpoint, but rather on the link leading to the endpoint.
"
	(define (get-link l)
		(define (apply-link e)
			(proc l)
		)
		(for-each apply-link (cog-filter endpoint-type (cog-outgoing-set l)))
	)
	(for-each get-link (cog-filter link-type (cog-incoming-set anchor)))
)

(define (cog-get-link link-type endpoint-type anchor)
"
  cog-get-link link-type endpoint-type anchor

  Return a list of links, of type 'link-type', which contain some
  atom of type 'endpoint-type', and also specifically contain the
  atom 'anchor'.

  Thus, for example, suppose the atom-space contains a link of the
  form
        (ReferenceLink
            (ConceptNode \"asdf\")
            (WordNode \"pqrs\")
        )
  Then, the call
     (cog-get-link 'ReferenceLink 'ConceptNode (WordNode \"pqrs\"))
  will return a list containing that link. Note that \"endpoint-type\"
  need not occur in the first position in the link; it can appear
  anywhere.
"
	(let ((lst '()))
		(define (mklist inst)
			(set! lst (cons inst lst))
			#f
		)
		(cog-map-apply-link link-type endpoint-type mklist anchor)
		lst
	)
)

; ---------------------------------------------------------------------
(define (cog-get-pred inst pred-type)
"
  cog-get-pred -- Find all EvaluationLinks of given form.

  Return a list of predicates, of the given type, that an instance
  participates in.  'inst' must be an atom, and 'pred-type' must be
  an atom type.  That is, given a \"predicate\" of the form:

     EvaluationLink
        SomeAtom
        ListLink
            AnotherAtom \"abc\"
            GivenAtom \"def\"

  then, given the instance 'inst' (in this example, GivenAtom \"def\")
  and predicate type 'pred-type' 'SomeAtom, then this routine returns
  a list of all of the EvalutaionLink's in which 'inst' appears.
"
	(concatenate!
		(append!
			(map
				(lambda (lnk) (cog-get-link 'EvaluationLink pred-type lnk))
				;; append removes null's
				(append! (cog-filter 'ListLink (cog-incoming-set inst)))
			)
		)
	)
)

; -----------------------------------------------------------------------
(define (cog-get-reference refptr)
"
  Given a reference structure, return the referenced list entries.
  That is, given a structure of the form

     ReferenceLink
         SomeAtom
         ListLink
            AnotherAtom
            AnotherAtom
            ...

  Then, given, as input, \"SomeAtom\", this returns a list of the \"OtherAtom\"

  XXX! Caution/error! This implictly assumes that there is only one
  such ReferenceLink in the system, total. This is wrong !!!
"
	(let ((lst (cog-chase-link 'ReferenceLink 'ListLink refptr)))
		(if (null? lst)
			'()
			(cog-outgoing-set (car lst))
		)
	)
)

; ---------------------------------------------------------------------
(define (filter-hypergraph pred? atom-list)
"
  filter-hypergraph -- recursively traverse outgoing links of graph.

  filter-hypergraph pred? atom-list

  Given a list of atoms, and a scheme-predicate pred?, return a
  list of atoms that satisfy the scheme-predicate.  This is not
  a simple srfi-1 'filter', rather, it traverses the hypergraph,
  applying the predicate to the subgraphs.

  In the current implementation, the scheme-predicate is assumed to
  select only for Nodes.
"
	(define (fv atoms lst)
		(cond
			; If its a query word, append it to the list
			((cog-node? atoms)
				(if (pred? atoms)
					(cons atoms lst)
					lst
				)
			)

			; If its a link, scan its outgoing set
			((cog-link? atoms)
				(fv (cog-outgoing-set atoms) lst)
			)

			; If its a list then scan the list
			((pair? atoms)
				(append!
					(append-map!
						(lambda (x) (filter-hypergraph pred? x)) atoms)
					lst
				)
			)
		)
	)
	(fv atom-list '())
)

; --------------------------------------------------------------------
(define (cartesian-prod tuple-of-lists)
"
  cartesian-prod -- create Cartesian product from tuple of sets.

  This returns the Cartestion product of a tuple of sets by distributing
  the product across the set elements. Returned is a list of tuples, where
  the elements of the tuple are elements from the corresponding sets.

  Let the input tuple be [s_1, s_2, ..., s_m] where each s_k is a set of
  one or more elements.  Let the cardinality of s_k be n_k.
  Then this returns a list of n_1 * n_2 *... * n_m tuples.
  where the projection of the coordinate k is an element of s_k.
  That is, let p_k be the k'th cordinate projection, so that
  p_k [a_1, a_2, ... , a_m] = a_k

  Then, if t is a tuple returned by this function, one has that
  p_k(t) is element of s_k for all t and all 1<= k <= m. Every possible
  combination of set elements is returned.

  Example usage and output:

  guile> (cartesian-prod (list 'p 'q))
  (p q)
  guile> (cartesian-prod (list 'p (list 'x 'y)))
  ((p x) (p y))
  guile> (cartesian-prod (list (list 'p 'q) 'x))
  ((p x) (q x))
  guile> (cartesian-prod (list (list 'p 'q) (list 'x 'y)))
  ((p x) (q x) (p y) (q y))
  guile> (cartesian-prod (list (list 'p 'q) 'a (list 'x 'y)))
  ((p a x) (q a x) (p a y) (q a y))
  guile> (cartesian-prod (list 'a (list 'p 'q) (list 'x 'y)))
  ((a p x) (a q x) (a p y) (a q y))

  XXX TODO -- this would almost surely be simpler to implement using
  srfi-1 fold or srfi-1 map, which is really all that it is ...
"

	; distribute example usage:
	; guile> (distribute (list 'p 'q)  (list '1 '2 '3))
	; ((p 1 2 3) (q 1 2 3))
	; guile> (distribute 'p (list '1 '2 '3))
	; (p 1 2 3)
	; guile> (distribute 'p '1)
	; (p 1)
	; guile> (distribute (list 'p 'q) '1)
	; ((p 1) (q 1))
	;
	(define (distribute heads a-tail)
		(define (match-up a-head tale)
			(if (list? tale)
				(cons a-head tale)
				(list a-head tale)
			)
		)
		(if (list? heads)
			(map (lambda (hed) (match-up hed a-tail)) heads)
			(match-up heads a-tail)
		)
	)

	; guile> (all-combos 'p (list 'x 'y))
	; ((p x) (p y))
	; guile> (all-combos 'p (list (list 'a 'b) (list 'y 'z)))
	; ((p a b) (p y z))
	; guile> (all-combos (list 'p 'q) (list 'x 'y))
	; ((p x) (q x) (p y) (q y))
	; guile> (all-combos (list 'p 'q) 'x)
	; ((p x) (q x))
	; guile> (all-combos 'p  'x)
	; (p x)
	;
	(define (all-combos heads tails)
		(if (list? tails)
			(let ((ll (map (lambda (tale) (distribute heads tale)) tails)))
				(if (list? heads)
					(concatenate! ll)
					ll
				)
			)

			; tails is not a list
			(distribute heads tails)
		)
	)

	(cond
		((null? tuple-of-lists) '())
		((not (list? tuple-of-lists)) tuple-of-lists)
		(else
			(let ((lc (car tuple-of-lists))
					(rc (cdr tuple-of-lists))
				)
				(if (null? rc)
					lc
					; cartesian-prod always returns tails, so we
					; just need to take all combinations of the
					; head against the returned tails.
					; (This is not a tail-recursive call but I don't care)
					(all-combos lc (cartesian-prod rc))
				)
			)
		)
	)
)

; ---------------------------------------------------------------------
(define (cartesian-prod-list-only tuple-of-lists)
"
  cartesian-prod-list-only -- Alternative version of cartesian-prod.

  Similar to cartesian-prod, but everything in 'tuple-of-lists' must be
  a list.  ie. cannot do (cartesian-prod-list-only (list 'p 'q))

  The different in this version is that given the input '(((1) (2)) ((3)))
  the above version returns (((1) 3)) ((2) 3)), but this version returns
  (((1) (3)) ((2) (3))).
"
	(fold-right
		; lst1: one of the list in the tuple
		; lst2: cartesian product of the rest
        	(lambda (lst1 lst2)
        		; choose each item in lst1 as 'head', and add it to the cartesian product of lst2
			(append-map
				(lambda (head)
					(if (null? lst2)
						(list (cons head '()))
						(map (lambda (rest-in-cart) (cons head rest-in-cart)) lst2)
					)
				)
				lst1
			)
        	)
		'()
		tuple-of-lists
	)
)

; ---------------------------------------------------------------------

; Returns true when x is equal to y up to an epsilon
(define (approx-eq? x y)
    (let ((diff (- x y))
          (minus-epsilon -0.000001)
          (plus-epsilon 0.000001)
          )
        (and (< minus-epsilon diff) (> plus-epsilon diff))
    )
)

; ---------------------------------------------------------------------
(define (cog-equal? atom-1 atom-2)
"
  Checks whether two nodes are equal. If they are equal then it will return
  TRUE_TV else it returns FALSE_TV.
"
    (if (equal? atom-1 atom-2)
        (stv 1 1)
        (stv 0 1)
    )
)

; ---------------------------------------------------------------------

; Given a list l and function k (from element to number) return the
; element so that k(e) is the lowest.
(define (min-element-by-key l k)
 (let ((head-element (car l)))
  (if (eq? (length l) 1)

   ; Base case
   head-element

   ; Recursive case
   (let* ((tail (cdr l))
          (head-key (k head-element))
          (tail-min-element (min-element-by-key tail k))
          (tail-min-key (k tail-min-element))
          )
    (if (< head-key tail-min-key)
     head-element
     tail-min-element
     )
    )
   )
  )
 )

; ---------------------------------------------------------------------

; Given a list l and function k (from element to number) return the
; element so that k(e) is the highest.
(define (max-element-by-key l k)
 (let ((head-element (car l)))
  (if (eq? (length l) 1)

   ; Base case
   head-element

   ; Recursive case
   (let* ((tail (cdr l))
          (head-key (k head-element))
          (tail-max-element (max-element-by-key tail k))
          (tail-max-key (k tail-max-element))
          )
    (if (< head-key tail-max-key)
     head-element
     tail-max-element
     )
    )
   )
  )
 )

; ---------------------------------------------------------------------


; A list of all the public (exported) utilities in this file
(define cog-utilities (list
'av
'stv
'itv
'ctv
'tv-mean
'tv-conf
'tv-count
'gar
'gdr
'gadr
'gddr
'gaddr
'gdddr
'for-each-except
'cog-atom-incr
'purge-hypergraph
'purge-type
'clear
'count-all
'cog-get-atoms
'cog-prt-atomspace
'cog-count-atoms
'cog-report-counts
'cog-get-root
'cog-get-all-nodes
'cog-get-partner
'cog-pred-get-partner
'cog-filter
'cog-chase-link
'cog-chase-link-chk
'cog-map-chase-link
'cog-par-chase-link
'cog-map-chase-links
'cog-par-chase-links
'cog-map-chase-links-chk
'cog-par-chase-links-chk
'cog-map-chase-link-dbg
'cog-map-apply-link
'cog-get-link
'cog-get-pred
'cog-get-reference
'filter-hypergraph
'cartesian-prod
'cartesian-prod-list-only
'approx-eq?
'cog-equal?
'min-element-by-key
'max-element-by-key
))

; Compile 'em all.  This should improve performance a bit.
; XXX FIXME This should be removed when guile-2.2 becomes popular
; (maybe in 2017?) since it compiles everything anyway.
(for-each
	(lambda (symb) (compile symb #:env (current-module)))
	cog-utilities
)
; ---------------------------------------------------------------------

; Call that in case you want to export those functions in a module. It
; is currently used by opencog.scm.
;
; TODO it would be better to use cog-utilities, however something like
;
; (for-each export cog-utilities)
;
; or
;
; (for-each (lambda (fun) (export fun)) cog-utilities)
;
; or
;
; (for-each (lambda (fun) (export (fun))) cog-utilities)
;
; Doesn't work, so till a fix is found I'm using this dedundant definition
;
(define (export-utilities) (export av stv itv ctv tv-mean tv-conf tv-count gar gdr gadr gddr gaddr gdddr for-each-except cog-atom-incr purge-hypergraph purge-type clear count-all cog-get-atoms cog-prt-atomspace cog-count-atoms cog-report-counts cog-get-root cog-get-all-nodes cog-get-partner cog-pred-get-partner cog-filter cog-chase-link cog-chase-link-chk cog-map-chase-link cog-par-chase-link cog-map-chase-links cog-par-chase-links cog-map-chase-links-chk cog-par-chase-links-chk cog-map-chase-link-dbg cog-map-apply-link cog-get-link cog-get-pred cog-get-reference filter-hypergraph cartesian-prod cartesian-prod-list-only approx-eq? cog-equal? min-element-by-key max-element-by-key))
