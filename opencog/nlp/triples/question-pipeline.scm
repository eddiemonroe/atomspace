;
; question-pipeline.txt
; 
; An experimental set of question-answering rules
;
; Copyright (c) 2009 Linas Vepstas <linasvepstas@gmail.com>
;
; -----------------------------------------------------------
; The following answers a simple WH-question (who, what, when etc.)
; The question is presumed to be a simple triple itself.
;
; To limit the scope of the search (for performance), the bottom
; of the prep-triple is assumed to be anchored.

# IF %ListLink("# TRIPLE BOTTOM ANCHOR", $qvar) 
		^ $prep($word-inst, $qvar)      ; the question
		^ &query_var($qvar)             ; validate WH-question
		^ %LemmaLink($word-inst, $word) ; common word-instance
		^ %LemmaLink($join-inst, $word) ; common seme
		^ $prep($join-inst, $ans)       ; answer
		^ ! &query_var($ans)            ; answer should NOT be a query
	THEN
      ^3_&declare_answer($ans)

# IF %ListLink("# TRIPLE BOTTOM ANCHOR", $qvar) 
		^ $prep($qvar, $var)    ; the question
		^ &query_var($qvar)     ; validate WH-question
		^ $prep($ans, $var)     ; answer
	THEN
      ^3_&declare_answer($ans)

