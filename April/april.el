;;; 
;;; APRIL Emacs mode
;;;

;;; NB: You can change the block indentation (default 2) by
;;;     inserting (setq april-block-indent 4) in your .emacs file
;;;

(setq april-xemacs (not (not (string-match "XEmacs" (emacs-version)))))

;;; Initialise the syntax table

(defun april-modify-syntax (table &rest pairs)
  (while pairs
    (modify-syntax-entry (car pairs) (cadr pairs) table)
    (setq pairs (cddr pairs))))

(defvar april-mode-syntax-table nil
  "Syntax table used while in April mode.")

(if april-mode-syntax-table 
    nil

  (setq april-mode-syntax-table (make-syntax-table))

  ;; Comments

  (april-modify-syntax april-mode-syntax-table
		       ?/   ". 14"
		       ?*   ". 23"
		       ?-   (if april-xemacs "w 56b" "w 12b")
		       ?\n  (if april-xemacs ">78b" ">56b"))


  ;; Symbols

  (april-modify-syntax april-mode-syntax-table
		       ?_   "w"    
		       ?+   "_"    
		       ?=   "w"    
		       ?%   "_"    
		       ?&   "_"    
		       ?|   "."    
		       ?\'  "w"
		       ?\>  "w"
		       ?\?  "."
		       ?\\  "\\"
		       ?^   "."
		       ?    "    "
		       ?\t  "    ")

  )

;;; Initialise the abbrev table

(defvar april-mode-abbrev-table nil
  "Abbrev table used while in April mode.")

(define-abbrev-table 'april-mode-abbrev-table ())

;;; Initialise the key map

(defvar april-mode-map nil)

(if april-mode-map 
    nil

  (setq april-mode-map (make-sparse-keymap))
  (define-key april-mode-map "\t" 'indent-for-tab-command)
  (define-key april-mode-map "" 'april-indent-sexp)
  (mapcar '(lambda (key-seq)
	     (define-key april-mode-map 
			 key-seq 
			 'april-self-insert-and-indent-command))
	  '("{" "}" ";" "|")))

(defun april-self-insert-and-indent-command (n)
  "Self insert and indent appropriately"
  (interactive "*P")
  (self-insert-command (prefix-numeric-value n))
  (indent-for-tab-command))

;;; Provide `april-mode' user callable function

(defun april-mode ()
  "Major mode for editing April programs"
  (interactive)
  (kill-all-local-variables)

  (use-local-map april-mode-map)
  (setq mode-name "April")
  (setq major-mode 'april-mode)

  (setq local-abbrev-table april-mode-abbrev-table)
  (set-syntax-table april-mode-syntax-table)

  ;; Local variables (comments)

  (make-local-variable 'comment-column)
  (setq comment-column 50)

  (make-local-variable 'comment-start)
  (setq comment-start "-- ")

  (make-local-variable 'comment-end)
  (setq comment-end "")

  (make-local-variable 'parse-sexp-ignore-comments)
  (setq parse-sexp-ignore-comments t)

;  (make-local-variable comment-start-skip)
  (setq comment-start-skip "--[ \t]*")

  ;; Local variables (indentation)

  (make-local-variable 'indent-line-function)
  (setq indent-line-function 'april-indent-line)

  ;; Initialise font-lock support

  (april-init-font-lock)
  (run-hooks 'april-mode-hook))

;;; Indentation

(defvar april-block-indent 2
  "* Amount by which to indent blocks of code in April mode")

;;; Relies on C/C++ mode's indentation functions
;;; for now.

(defun april-find-open-brace (pos)
  (save-excursion
    (goto-char pos)
    (condition-case nil
	(scan-lists (point) -1 1)
      (error nil))))

(defun april-indentation-level (pos)
  (save-excursion
    (goto-char pos)
    (beginning-of-line)
    (let ((bol (point)))
      (skip-chars-forward " \t")
      (current-column))))

;;; This function is borrowed from cc-mode:c-backward-syntactic-ws
;;; (cc-engine.el)

(defun april-backward-syntactic-ws (&optional lim)
  ;; Backward skip over syntactic whitespace for Emacs 19.
  (let* ((here (point-min))
	 (hugenum (- (point-max))))
    (while (/= here (point))
      (setq here (point))
      (forward-comment hugenum))
    (if lim 
	(goto-char (max (point) lim)))))

(defun april-backward-word ()
  (april-backward-syntactic-ws)
  ;; (backward-word 1)
  (skip-syntax-backward "w")		; Can cause problems with indentation
)

(defun april-previous-word-was (regexp)
  (save-excursion
    (april-backward-word)
    (looking-at regexp)))
  
(defun april-find-previous-word (pos)
  (save-excursion
    (goto-char pos)
    (april-backward-word) 
    (point)))

(defun april-indentation-column (pos)
  (save-excursion
    (goto-char pos)

    (cond
     ((looking-at "{")     (april-indentation-level pos))
     (t                    (current-column)))))
	  
(defun april-line-begins-with (pos what)
  (save-excursion
    (goto-char pos)
    (beginning-of-line)
    (skip-chars-forward " \t")
    (looking-at what)))

(defun april-begins-with (pos what)
  (save-excursion
    (goto-char pos)
    (skip-chars-forward " \t")
    (looking-at what)))

(if (boundp 'point-at-eol) 
    (setf april-point-at-eol 'point-at-eol)
  (defun april-point-at-eol ()
    (save-excursion
      (end-of-line) 
      (point))))

(defun april-paren-indent (pos)
  ;; This function can be simplified
  (save-excursion
    (goto-char pos)
    (let ((eol      (april-point-at-eol))
	  (here	    (point))
	  (here-col (current-column)))
      (progn 
	(forward-char)
	(skip-chars-forward " \t")
	(cond
	 ((> (point) eol)
	  (goto-char (point))
	  (1+ (current-column)))
	    
	 (t (current-column))))
      )))
     
(defun april-one-of (&rest l)
  (if (cadr l) 
      (concat (car l) "\\|"
	      (apply 'april-one-of (cdr l)))
    (car l)))

(defvar april-double-indent-keywords 
  (april-one-of "then" "else" "do" "repeat" "try" "onerror" "->" "->>" "=>" "::=")
  "April double indent keywords")

(defvar april-hanging-parentheses 
  "[\\[(]"
  "April hanging parentheses regexp")

(defvar april-no-hanging-parentheses 
  "[\\[(][ \t]*$"
  "April no-hanging parentheses regexp")

(defvar april-comment "\\(--\\|/\\*\\)"
  "April comment")

(defvar april-comment-bol (concat "^" april-comment)
  "April comment at beginning of line")

(defun april-calculate-brace-indent (pos)
  (save-excursion
    (let ((upper-level (april-find-open-brace pos)))
      (if upper-level
	  (cond ((and (april-begins-with upper-level april-hanging-parentheses)
		      (not (april-begins-with upper-level april-no-hanging-parentheses)))
		 (april-paren-indent upper-level))

		(t (+ april-block-indent (april-calculate-brace-indent upper-level))))
	0))))

(defun april-calculate-indent (pos)
  (let ((previous-line (april-find-open-brace pos))
	(previous-word (april-find-previous-word pos))
	(brace-indent  (april-calculate-brace-indent pos)))

    (if previous-line
	(let ((curlevel (- brace-indent april-block-indent))) 
	  (+ curlevel 
	     (cond
	      ((april-line-begins-with pos april-comment-bol)     (- curlevel))
	      ((april-line-begins-with pos april-comment)         april-block-indent)
	      ((april-line-begins-with pos "}")                   0) 
	      ((april-line-begins-with pos ")")                   (- april-block-indent 1)) 
	      ((april-line-begins-with pos "|")                   (- april-block-indent 2))

	      ((and (> previous-word previous-line)
		    (april-previous-word-was 
		     april-double-indent-keywords))               (* april-block-indent 2))

	      (t                                                  april-block-indent))))
      0)))

(defun april-goto-first-non-whitespace-maybe ()
  (let ((dest (save-excursion
		(beginning-of-line)
		(skip-chars-forward " \t")
		(point))))
    (if (< (point) dest)
	(goto-char dest))))

(defun april-indent-line ()
  (interactive)
  (save-excursion
    (let* ((cur-level   (april-indentation-level (point)))
	   (bol         (progn (beginning-of-line) (point)))
	   (level       (april-calculate-indent bol ;(point)  
						)))
	
      (if (= cur-level level)
	  nil

	(delete-horizontal-space)
	(indent-to level))))

  (april-goto-first-non-whitespace-maybe))

(defun april-indent-sexp ()
  (interactive)
  (save-excursion
    (let ((start  (point))
	  (stop   (condition-case nil
		      (save-excursion (forward-sexp 1) (point))
		    (error (point)))))

      (indent-region start stop nil))))
		    
;;; Font-lock support

(defvar april-font-lock-keyword-regexp 
  (concat "\\b\\("
	  (april-one-of 
	   "while"			; control
	   "where"			; sugar
	   "void"			; type
	   "valof"			; form
	   "valis"			; form
	   "using"			; 
	   "until"			; control
	   "type"			; keyword
	   "try"			; control
	   "true"			; 
	   "timeout"			;
	   "timedout"			;
	   "clickedout"			;
	   "then"			; control
	   "char"			; type
	   "symbol"			; type
	   "string"			; type
	   "step"			; 
	   "spawn"			; control
	   "signed"			; 
	   "setof"			; form
	   "repeat"			; control
	   "receive"			; message
	   "program"			; 
	   "onerror"			; control
	   "of"				; sugar
	   "object"			; type
	   "number"			; type 
	   "module"			;
	   "logical"			; type
	   "let"			; control
	   "leave"			; control
	   "istrue"			;
	   "interface"			; module
	   "in"				; module
	   "import"			; module
	   "and"			; module
	   "native"			; module
	   "if"				; control
	   "handle"			; type
	   "generator"
	   "from"			; module
	   "forall"			; control
	   "for"			; control
	   "false"			;
	   "failed"			;
	   "export"			; module
	   "execute"			; module
	   "exception"			; control
	   "error"			; control
	   "environment"
	   "else"			; control
	   "elemis"			; form
	   "collect"			; form
	   "do"				; control
	   "case"			; control
	   "bagof"			; form
	   "as"
	   "any"
	   "alarm"
           "within")
	  "\\)\\b")
  "Regular expression matching the keywords to highlight in April mode")

(defvar april-font-lock-symbol-regexp
  (concat "\\("
	  (april-one-of "~~"
			"=>"
;;;			"~"
;;;			"\\^"
			"\\?\\?"
			"\\.\\."
			"\\.="
			">>\\*"
			">>>"
			">>"
			"<<"
			">="
			"==>"
;;;			"=="
;;;			"="
;;;			"<="
			"::="
			"::"
			":-"
;;;			":"
			"->>"
			"->"
			",\\.\\."
			"''?"
			"%"
			"!>>"
			"!"
			"!!")
	  "\\)")
  "Regular expression matching the symbols to highlight in April mode")

(defvar april-font-lock-function-regexp
  "^[ \t]*\\(\\sw+\\)([0-9_a-zA-Z?, ]*)[ \t]*=>"
;;;  "^[ \t]*\\(\\sw+\\)([0-9_a-zA-Z?, ]*"
  "Regular expression matching the function declarations to highlight in April mode")

(defvar april-font-lock-directive-regexp
  "^\\(#[a-zA-Z]+\\)"
  "Regular expression matching the compiler directives to highlight in April mode")

(defvar april-font-lock-include-regexp
  "interface[ \t]+<\\([a-zA-Z0-9_/.]+\\)"
  "Regular expression matching the compiler include files")

(defconst april-font-lock-keywords-1
  `((,april-font-lock-keyword-regexp   (1 font-lock-keyword-face))
;;;    (,april-font-lock-function-regexp  (1 font-lock-function-name-face))
    (,april-font-lock-symbol-regexp    (1 font-lock-reference-face))
    (,april-font-lock-include-regexp   (1 font-lock-string-face))
    (,april-font-lock-directive-regexp (1 ,(if (boundp 'font-lock-preprocessor-face)
				     'font-lock-preprocessor-face
				     'font-lock-reference-face)))
)
  "Keywords to syntax highlight with font-lock-mode")

(defvar april-font-lock-keywords april-font-lock-keywords-1
  "Keywords to syntax highlight with font-lock-mode")

(defun april-init-font-lock ()
  (make-local-variable 'font-lock-defaults)
  (setq font-lock-defaults '(april-font-lock-keywords)))

(provide 'april)
