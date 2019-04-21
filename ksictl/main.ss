(define-record-type dynamic-vector
  (fields (mutable length)
          (mutable content-vector))
  (protocol
   (lambda (p)
     (lambda ()
       (p 0 (make-vector 4))))))
(define (dynamic-vector-ref vec index default)
  (if (< index (dynamic-vector-length vec))
      (vector-ref (dynamic-vector-content-vector vec) index)
      default))
(define (dynamic-vector-set! vec index val)
  (if (< index (dynamic-vector-length vec))
      (vector-set! (dynamic-vector-content-vector vec) index val)
      (assertion-violation 'dynamic-vector-set! "index out of range" vec index val)))
(define (dynamic-vector-resize vec new-size)
  (define content (dynamic-vector-content-vector vec))
  (define old-cap (vector-length content))
  (dynamic-vector-length-set! vec new-size)
  (if (> new-size old-cap)
      (letrec ([new-content (make-vector (* 2 old-cap))]
               [h (lambda (src-vec des-vec idx)
                    (if (< idx old-cap)
                        (begin
                          (vector-set! des-vec idx (vector-ref src-vec idx))
                          (h src-vec des-vec (+ idx 1)))))])
        (h content new-content 0)
        (dynamic-vector-content-vector-set! vec new-content))))
(define-record-type wire-key
  (fields (mutable source-name)
          (mutable source-port)
          (mutable destination-name)
          (mutable destination-port)))
(define (wire-key-hash wk)
  (bitwise-xor (string-hash (wire-key-source-name wk))
               (string-hash (wire-key-destination-name wk))
               (wire-key-source-port wk)
               (wire-key-destination-port wk)))
(define (wire-key-equal? a b)
  (define (wire-accessor-equal? p)
    (equal? (p a) (p b)))
  (and (wire-accessor-equal? wire-key-source-name)
       (wire-accessor-equal? wire-key-destination-name)
       (wire-accessor-equal? wire-key-source-port)
       (wire-accessor-equal? wire-key-destination-port)))
(define-record-type graph-prototype
  (fields (mutable instances)
          (mutable node-table)
          (mutable wire-table))
  (protocol
   (lambda (p)
     (lambda (instance)
       (p (list instance)
          (make-hash-table string-hash equal?)
          (make-hash-table wire-key-hash wire-key-equal?))))))
(define-record-type graph-instance
  (fields (mutable node-map-table)
          (mutable ksid-port)
          (mutable ksid-ret-port))
  (protocol
   (lambda (p)
     (lambda (ksid-port ksid-ret-port)
       (p (make-hash-table string-hash equal?) ksid-port ksid-ret-port)))))
(define-record-type general-node-port
  (fields (mutable type)
          (mutable wires))
  (protocol
   (lambda (p)
     (lambda (t)
       (p t (make-hash-table wire-key-hash wire-key-equal?))))))
(define-record-type general-node
  (fields (mutable attribute-table)
          (mutable inlets)
          (mutable outlets))
  (protocol
   (lambda (p)
     (lambda () (p (make-hashtable string-hash equal?)
                   (make-dynamic-vector)
                   (make-dynamic-vector))))))
(define-record-type dsp-node
  (parent general-node)
  (protocol
   (lambda (n)
     ((n)))))
(define-record-type subgraph-node
  (parent general-node)
  (protocol
   (lambda (n)
     ((n)))))
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
  (newline out-port))
(define (graph-prototype-node-get prototype node-name)
  (hashtable-ref (graph-prototype-node-table prototype)
                 node-name
                 #f))
(define (graph-prototype-node-field-set! prototype node-name field-name val)
  (define node (graph-prototype-node-get prototype node-name))
  (hashtable-set! (general-node-attribute-table node)
                  field-name val))
(define (graph-prototype-node-field-get prototype node-name field-name)
  (hashtable-ref (general-node-attribute-table
                  (graph-prototype-node-get prototype node-name))
                 field-name))