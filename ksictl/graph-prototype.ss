(include "vec.ss")
(include "graph-instance.ss")
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


(define-record-type general-node-port
  (fields (mutable type)
          (mutable wires))
  (protocol
   (lambda (p)
     (lambda (t)
       (p t (make-hash-table wire-key-hash wire-key-equal?))))))

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

(define (char-to-type c)
  (case c
    [#\i 'int]
    [#\f 'float]
    [#\I 'int-event]
    [#\F 'float-event]))


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
