;
; file-utils.scm
; Assorted file and directory utils.
;
; The following utilities are provided in this file:
;
; -- list-files dir                List files in a directory
; -- exec-scm-from-port port       Execute scheme code read from port
; -- exec-scm-from-cmd cmd-string  Run scheme returned by shell command
; -- load-scm-from-file filename   Run scheme code taken from file
; -- export-all-atoms filename     Export entire atomsapce into file
;
; Copyright (c) 2008, 2013 Linas Vepstas
;

(use-modules (ice-9 rdelim))
(use-modules (ice-9 popen))
(use-modules (ice-9 rw))
(use-modules (rnrs io ports))

; ---------------------------------------------------------------------
(define-public (list-files dir)
"
 list-files dir    List files in a directory

 Given a directory, return a list of all of the files in the directory
 Do not return anything that is a subdirectory, pipe, special file etc.
"

	(define (isfile? file)
		(eq? 'regular (stat:type (stat
				(string-join (list dir file) "/")))
		)
	)

	; suck all the filenames off a port
	(define (suck-in-filenames port lst)
		(let ((one-file (readdir port)))
			(if (eof-object? one-file)
				lst
				(suck-in-filenames port
					(if (isfile? one-file)
						(cons one-file lst)
						lst
					)
				)
			)
		)
	)
	(let* ((dirport (opendir dir))
			(filelist (suck-in-filenames dirport '()))
		)
		(closedir dirport)
		filelist
	)
)

; ---------------------------------------------------------------------
(define-public (exec-scm-from-port port)
"
 exec-scm-from-port port   Execute scheme code read from port

 Read (UTF-8 encoded) data from the indicated port, and run it.
 The port should contain valid scheme; this routine will read and
 execute that scheme data.

 CAUTION: This routine will hang until the remote end closes the port.
 That is, it will continue to attempt to read more data from the port,
 until an EOF is received.  For sockets, an EOF is sent only when the
 remote end closes its transmit port.
"

	; get-string-all is a new r6rs proceedure, sucks in all bytes until
	; EOF on the port. Seems like TCP/IP ports end up being textual in
	; guile, and the default r6rs transcoder is UTF8 and so everyone
	; is happy, these days.  Note: in the good-old bad days, we used
	; ice-9 rw read-string!/partial for this, which went buggy, and
	; started mangling at some point.
	(let ((string-read (get-string-all port)))
		(if (eof-object? string-read)
			#f
			(eval-string string-read)
		)
	)
)

; ---------------------------------------------------------------------
(define-public (exec-scm-from-cmd cmd-string)
"
 exec-scm-from-cmd cmd-string   Run scheme returend by shell command

 Load data generated by the system command cmd-string. The command
 should generate valid scheme, and send its data to stdout; this
 routine will read and execute that scheme data.

 Example:
 (exec-scm-from-cmd \"cat /tmp/some-file.scm\")
 (exec-scm-from-cmd \"cat /tmp/some-file.txt | perl ./bin/some-script.pl\")
"

	(let ((port (open-input-pipe cmd-string)))
		(exec-scm-from-port port)
		(close-pipe port)
	)
)

; ---------------------------------------------------------------------
; XXX this duplicates (load-from-path) which is a built-in in guile...
(define-public (load-scm-from-file filename)
"
 load-scm-from-file filename   Run scheme code taken from file

 Load scheme data from the indicated file, and execute it.

 Example Usage:
 (load-scm-from-file \"/tmp/some-file.scm\")
"
	(exec-scm-from-cmd (string-join (list "cat \"" filename "\"" ) ""))
)

; ---------------------------------------------------------------------
(define-public (prt-atom-list port lst)
"
 prt-atom-list          Send to port the list of atoms.

 Sends a list of atoms 'lst' to the open port 'port'.
"
    (if (not (null? lst))
        ; try to protect against undefined handles, although note that
        ; this isn't guaranteed as some other process could delete the
        ; handle between the if statement and the display statement
        ; the only way to truely fix this is to block the atomspace
        (if (not (eq? (cog-undefined-handle) (car lst)))
            (let()
                (if (= (length (cog-incoming-set (car lst))) 0)
                    (display (car lst) port)
                )
                (prt-atom-list port (cdr lst))
            )
        )
    )
)

; ---------------------------------------------------------------------
(define-public (export-atoms lst filename)
"
 export-atoms           Export the atoms in a list to file.

 Exports the list of atoms to the file 'filename'. If an absolute/relative
 path is not specified, then the filename will be written to the directory
 in which the opencog server was started.
"
    (let ((port (open-file filename "w")))
        (prt-atom-list port lst)
        (close-port port)
    )
)

; ---------------------------------------------------------------------
(define-public (export-all-atoms filename)
"
 export-all-atoms filename      Export entire atomspace into file

 Export the entire contents of the atomspace to the file 'filename'
 If an absolute path is not specified, then the filename will be
 written to the directory in which the opencog server was started.
"
	(let ((port (open-file filename "w"))
		)
		(for-each
			(lambda (x) (prt-atom-list port (cog-get-atoms x)))
			(cog-get-types)
		)

		(close-port port)
	)
)

; ---------------------------------------------------------------------
