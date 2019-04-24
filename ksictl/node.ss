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
