(define-record-type graph-instance
  (fields (mutable node-map-table)
          (mutable ksid-port)
          (mutable ksid-ret-port))
  (protocol
   (lambda (p)
     (lambda (ksid-port ksid-ret-port)
       (p (make-hash-table string-hash equal?) ksid-port ksid-ret-port)))))
(define (get-string in-port)
  (define (h in-port str-tail)
    (define next-char (get-char in-port))
    (if (not (equal? next-char #\;))
        (let ([next-pair (list next-char)])
          (set-cdr! str-tail next-pair)
          (h in-port next-pair))))
  (define str-list (list '()))
  (h in-port str-list)
  (list->string (cdr str-list)))

(define (graph-instance-add-node instance node-name component-id)
  (define out-port (graph-instance-ksid-port instance))
  (define in-port (graph-instance-ksid-ret-port instance))
  (put-string out-port "nn")
  (put-datum out-port component-id)
  (newline out-port)
  (if (equal? (get-string in-port) "0")
      (begin
        (get-string in-port);; error msg. should be empty
        (hashtable-set! (graph-instance-node-map-table instance) node-name (string->number (get-string in-port)));; node ID
        (list 'success (get-string in-port) (get-string in-port)))
      (list 'failed (get-line in-port))))
