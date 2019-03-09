#lang racket/gui

(struct node-port (type
                          [highlight #:mutable]
                          [wires #:mutable]))
(define (is-signal-port? p) (eq? (node-port-type p) 'signal))
(define (is-event-port? p) (eq? (node-port-type p) 'event))
(define (can-connect? p1 p2)
  (apply eq? (map node-port-type (list p1 p2))))
(define (new-node-port type) (node-port type #f '()))

(define block-size 30)
(define round-size (* block-size 0.25))
(define port-radius (* block-size 0.1))
(define port-mouse-radius (* block-size 0.2))
(define mouse-unmoved-threshold (exact-ceiling (* block-size 0.1)))
(define border-width (* block-size 0.05))
(define border-color (make-color 100 100 100))
(define grid-width (* block-size 0.05))
(define grid-color (make-color 50 50 50))
(define background-color (make-color 30 30 30))
(define wire-width (* block-size 0.1))
(define wire-curve-ratio 1/4)
(define wire-curve-coratio (- 1 wire-curve-ratio))
(define signal-port-color (make-color 0 171 173))
(define event-port-color (make-color 105 134 206))
(define (port-color p) (cond ((is-signal-port? p) signal-port-color)
                             ((is-event-port? p) event-port-color)
                             ('t (raise "Unknown port type"))))
(define port-highlight-radius (* block-size 0.5))
(define port-highlight-zoom-radius (* block-size 0.15))
(define node-margin (inexact->exact (* block-size 0.5)))
(define node-text-margin (inexact->exact (* block-size 0.7)))
(define wire-margin (* block-size 0.2))
(define (make-port-highlight-brush r g b)
  (new brush% [gradient
               (new radial-gradient%
                    [x0 0] [y0 0] [r0 port-radius]
                    [x1 0] [y1 0] [r1 port-highlight-radius]
                    [stops
                     (list (list 0   (make-color r g b 0.7))
                           (list 0.4   (make-color r g b 0.2))
                           (list 0.7   (make-color r g b 0.05))
                           (list 1 (make-color r g b 0)))])]))
(define signal-port-highlight-brush (make-port-highlight-brush 0 171 173))
(define event-port-highlight-brush (make-port-highlight-brush 105 134 206))
(define (port-highlight-brush t) (cond [(is-signal-port? t) signal-port-highlight-brush]
                                       [(is-event-port? t) event-port-highlight-brush]
                                       ))
(define f (new frame% [label "Simple Edit"]
               [width 1000]
               [height 1000]))
(define c (new editor-canvas% [parent f]))
(define graph-board%
  (class pasteboard%
    (init-field [incomplete-wire #f]
                [drag-x #f]
                [drag-y #f]
                [selecting #f])
    (super-new)
    (define/override (on-paint before? dc . other)
      (when before?
        (draw-background dc)))
    (define (draw-background dc)
      (define x (box 0))
      (define y (box 0))
      (define w (box 0))
      (define h (box 0))
      (define (draw-line x1 y1 x2 y2)
        (define-values (dx1 dy1) (send this editor-location-to-dc-location x1 y1))
        (define-values (dx2 dy2) (send this editor-location-to-dc-location x2 y2))
        (send dc draw-line dx1 dy1 dx2 dy2))
      (send (send this get-admin) get-view x y w h #f)
      (send dc clear)
      (send dc set-smoothing 'smoothed)
      (send dc set-background background-color)
      (send dc set-pen grid-color grid-width 'solid)
      (let ([x (unbox x)]
            [y (unbox y)]
            [w (unbox w)]
            [h (unbox h)])
          (let ([dy (* (exact-ceiling (/ y block-size)) block-size)]
            [dx (* (exact-ceiling (/ x block-size)) block-size)])
        (for ([i (in-range (exact-ceiling (/ h block-size)))])
        (let ([y (+ dy (* block-size i))])
          (draw-line dx y (+ dx w) y)))
        (for ([i (in-range (exact-ceiling (/ w block-size)))])
        (let ([x (+ dx (* block-size i))])
          (draw-line x dy x (+ dy h)))))))
    (define/augment (after-select snip on?)
      (when (and on? (not incomplete-wire))
        (send snip bring-all-wire-to-front this)
        (send this set-before snip #f)
        ))
    (define/augment (on-select snip on?)
      (when (and on? (not incomplete-wire))
        (set! selecting #t)))
    (define/augment (can-select? snip on?)
      (equal? node-snip-class (send snip get-snipclass)))
    (define/augment (after-interactive-move event)
      (define sx (box 0))
      (define sy (box 0))
      (define s1 (send this find-next-selected-snip #f))
      (when (and (< (abs (- (send event get-x) drag-x)) mouse-unmoved-threshold) (< (abs (- (send event get-x) drag-x)) mouse-unmoved-threshold))
        (cond
              [(not selecting) (send this set-caret-owner (let-values ([(event-edx event-edy) (send this dc-location-to-editor-location
                                                                                       (send event get-x)
                                                                                       (send event get-y))]) (send this find-snip event-edx event-edy)))]))
      (when s1 (send this get-snip-location s1 sx sy)
            (let ([dx (- (* (exact-round (/ (+ node-margin (unbox sx)) block-size)) block-size) (+ node-margin (unbox sx)))]
                  [dy (- (* (exact-round (/ (+ node-margin (unbox sy)) block-size)) block-size) (+ node-margin (unbox sy)))])
              (letrec ([h (lambda (s)
                            (when s (send this get-snip-location s sx sy)
                                  (send this move-to s (+ (unbox sx) dx) (+ (unbox sy) dy))
                                  (h (send this find-next-selected-snip s))))])
                (h s1))))
      (set! selecting #f))
    (define/augment (on-interactive-move event)
      (set! drag-x (send event get-x))
      (set! drag-y (send event get-y)))
    (define/override (on-double-click snip event)
      '())
    (define/augment (can-interactive-move? event)
      (not incomplete-wire))
    (define/override (find-snip x y [after #f])
      (let ([s (super find-snip x y after)])
        (if s (if (equal? (send s get-snipclass) node-snip-class)
                  s
                  (send this find-snip x y s))
            #f)))
    (define/augment (after-move-to snip x y dragging)
      (when (equal? node-snip-class (send snip get-snipclass))
          (send snip adjust-all-wire-location x y)))
    (define/override (on-event event)
      (define-values (event-edx event-edy) (send this dc-location-to-editor-location
                                                 (send event get-x)
                                                 (send event get-y)))
      (define (handle-port-event in out) (let ([s (send this find-snip
                                                        event-edx event-edy)]
                                               [x (box 0)]
                                               [y (box 0)])
                                           (when s (send this get-snip-location s x y)
                                                 (let ([ux (unbox x)]
                                                       [uy (unbox y)])
                                                   (let-values ([(dcx dcy) (send this editor-location-to-dc-location ux uy)])
                                                     (send s handle-port-event
                                                     (lambda (a b)
                                                             (in a b s ux uy))
                                                     (lambda (a b)
                                                             (out a b s ux uy))
                                                     dcx dcy event))))))
      (if incomplete-wire
          (cond [(send event moving?)
                 (let ([dc-event-x (send event get-x)]
                       [dc-event-y (send event get-y)])
                   (let-values ([(ex ey) (send this dc-location-to-editor-location dc-event-x dc-event-y)])
                     (if (get-field inlet-node incomplete-wire)
                     (begin
                       (set-field! outlet-x incomplete-wire ex)
                       (set-field! outlet-y incomplete-wire ey))
                     (begin
                       (set-field! inlet-x incomplete-wire ex)
                       (set-field! inlet-y incomplete-wire ey)))
                   (send incomplete-wire update this)
                   (handle-port-event (lambda (port-id port s x y) (send s set-highlight-inlet port-id))
                                      (lambda (port-id port s x y) (send s set-highlight-outlet port-id)))))]
                [(send event button-down?)
                 (handle-port-event (lambda (port-id port s x y) (send s adjust-wire-location x y incomplete-wire port-id 'inlet)
                                            (let ([p1 (get-field outlet-port incomplete-wire)])
                                                               (when (and p1 (can-connect? p1 port))
                                                             (set-field! inlet-node incomplete-wire s)
                                                             (set-field! inlet-port incomplete-wire port)
                                                             (set-node-port-wires! port
                                                                                   (cons incomplete-wire
                                                                                         (node-port-wires port)))
                                                             (set! incomplete-wire #f))))
                                    (lambda (port-id port s x y) (send s adjust-wire-location x y incomplete-wire port-id 'outlet)
                                            (let ([p1 (get-field inlet-port incomplete-wire)])
                                                                  (when (and p1 (can-connect? p1 port))
                                                             (set-field! outlet-node incomplete-wire s)
                                                             (set-field! outlet-port incomplete-wire port)
                                                             (set-node-port-wires! port
                                                                                   (cons incomplete-wire
                                                                                         (node-port-wires port)))
                                                             (set! incomplete-wire #f)))))
                 (when incomplete-wire (remove-wire incomplete-wire)
                       (set! incomplete-wire #f))])
          (super on-event event)))
    (define/public (remove-wire wire)
      (send this remove wire)
      (let ([n (get-field inlet-node wire)]
            [p (get-field inlet-port wire)])
        (when n
          (set-node-port-wires! p (remove wire (node-port-wires p)))
          (send n refresh-inlet)))
      (let ([n (get-field outlet-node wire)]
            [p (get-field outlet-port wire)])
        (when n
          (set-node-port-wires! p (remove wire (node-port-wires p)))
          (send n refresh-outlet))))
    ))
(define pb (new graph-board%))
(define node-snip-class%
  (class snip-class%
    (inherit set-classname)
    (super-new)
    ))
(define node-snip-class (new node-snip-class%))
(define wire-snip-class%
  (class snip-class%
    (inherit set-classname)
    (super-new)
    ))
(define wire-snip-class (new wire-snip-class%))
(define (within-range x a b) (and (< x b) (> x a)))
(define (within-range-around x center r) (within-range x (- center r) (+ center r)))
(define wire-snip%
  (class snip% (inherit set-snipclass
                        get-flags set-flags
                        get-admin)
  (init-field [inlet-x 0]
              [inlet-y 0]
              [outlet-x 0]
              [outlet-y 0]
              [inlet-node #f]
              [inlet-port #f]
              [outlet-node #f]
              [outlet-port #f])
  (super-new)
    (set-snipclass wire-snip-class)
    (set-flags '())
  (send (get-the-snip-class-list) add wire-snip-class)
    (define/public (update pb)
      (send pb move-to this (- (min inlet-x outlet-x) wire-margin) (- (min inlet-y outlet-y) wire-margin))
      (send pb resized this #t))
  (define/override (get-extent dc x y
                               [w #f]
                               [h #f]
                               [descent #f]
                               [space #f]
                               [lspace #f]
                               [rspace #f])
    (define (maybe-set-box! b v) (when b (set-box! b v)))
    (maybe-set-box! w (+ (* wire-margin 2) (abs (- inlet-x outlet-x))))
    (maybe-set-box! h (+ (* wire-margin 2) (abs (- inlet-y outlet-y))))
    (maybe-set-box! descent 0)
    (maybe-set-box! space 0)
    (maybe-set-box! lspace 0)
    (maybe-set-box! rspace 0))
    (define/override (draw dc x y left top right bottom dx dy draw-caret)
      (define ed (send (send this get-admin) get-editor))
      (define-values (dc-inlet-x dc-inlet-y) (send ed editor-location-to-dc-location inlet-x inlet-y))
      (define-values (dc-outlet-x dc-outlet-y) (send ed editor-location-to-dc-location outlet-x outlet-y))
      (define wp (new dc-path%))
      (define midy (/ (+ dc-inlet-y dc-outlet-y) 2))
      (send wp move-to dc-inlet-x dc-inlet-y)
      (send wp curve-to (+ (* dc-outlet-x wire-curve-ratio) (* dc-inlet-x wire-curve-coratio)) midy (+ (* dc-outlet-x wire-curve-coratio) (* dc-inlet-x wire-curve-ratio)) midy dc-outlet-x dc-outlet-y )
      (send dc set-pen (port-color (if inlet-port inlet-port outlet-port)) wire-width 'solid)
      (send dc set-brush border-color 'transparent)
      (send dc draw-path wp))))
(define node-text-style-delta (new style-delta%))
(send node-text-style-delta set-delta-foreground border-color)
(send node-text-style-delta set-transparent-text-backing-on #t)
(send node-text-style-delta set-transparent-text-backing-off #f)
(define node-text%
  (class text%
    (super-new)
    (send this change-style node-text-style-delta)
    (define/override (on-paint before? dc . other)
      (when before?
        (send dc set-background background-color)
      (send dc clear)))))
(define node-snip%
  (class editor-snip%
    (inherit set-snipclass
             get-flags set-flags
             get-admin)
    (init-field [xblocks 10]
                [yblocks 1]
                [extended #f]
                [inlets (list (new-node-port 'signal))]
                [outlets (list (new-node-port 'event))])
    (super-new [with-border? #f])
    (set-snipclass node-snip-class)
    (send (get-the-snip-class-list) add node-snip-class)
    (set-flags (cons 'handles-all-mouse-events (get-flags)))
    (let ([tx (new node-text%)])
      (send this set-editor tx))
    (define/override (get-extent dc x y
                                 [w #f]
                                 [h #f]
                                 [descent #f]
                                 [space #f]
                                 [lspace #f]
                                 [rspace #f])
      (define (maybe-set-box! b v) (when b (set-box! b v)))
      (define uw (+ (* xblocks block-size) (* node-margin 2)))
      (define uh (+ (* yblocks block-size) (* node-margin 2)))
      (maybe-set-box! w uw)
      (maybe-set-box! h uh)
      (maybe-set-box! descent node-margin)
      (maybe-set-box! space node-margin)
      (maybe-set-box! lspace node-margin)
      (maybe-set-box! rspace node-margin)
      (send this resize uw uh)
      (send this set-margin node-text-margin node-text-margin node-text-margin node-text-margin))

    (define/override (draw dc x y left top right bottom dx dy draw-caret)
      (define (xcoord xb) (+ x node-margin (* xb block-size)))
      (define (ycoord yb) (+ y node-margin (* yb block-size)))
      (define (draw-port num p y)
        (define h-rad port-highlight-radius)
        (define h-size (* h-rad 2))
        (define h? (node-port-highlight p))
        (define rad (if h? port-highlight-zoom-radius port-radius))
        (define size (* rad 2))
        (define cx (xcoord (+ num 1)))
        (define c (port-color p))
        (when (node-port-highlight p)
          (let-values ([(ox oy) (send dc get-origin)])
          (send dc set-pen c border-width 'transparent)
          (send dc set-brush (port-highlight-brush p))
          (send dc set-origin cx y)
          (send dc draw-ellipse (- 0 h-rad) (- 0 h-rad) h-size h-size)
          (send dc set-origin ox oy)))
        (send dc set-pen c border-width 'solid)
        (send dc set-brush (if (null? (node-port-wires p)) background-color c) 'solid)
        (send dc draw-ellipse (- cx rad) (- y rad) size size))
      (send dc set-pen border-color border-width 'solid)
      (send dc set-brush background-color 'solid)
      (send dc draw-rounded-rectangle (xcoord 0) (ycoord 0) (* xblocks block-size) (* yblocks block-size) round-size)
      (super draw dc x y left top right bottom dx dy draw-caret)
            (for ([(p i) (in-indexed inlets)])
              (draw-port i p (ycoord 0)))
            (for ([(p i) (in-indexed outlets)])
              (draw-port i p (ycoord yblocks))))
    (define/override (copy)
      (new node-snip% [xblocks yblocks]))
    (define/public (adjust-wire-location x y wire port-id in-or-out)
      (define (xcoord xb) (+ x node-margin (* xb block-size)))
      (define (ycoord yb) (+ y node-margin (* yb block-size)))
      (cond [(eq? in-or-out 'inlet)
             (set-field! inlet-x wire (xcoord (+ port-id 1)))
             (set-field! inlet-y wire (ycoord 0))]
            [(eq? in-or-out 'outlet)
             (set-field! outlet-x wire (xcoord (+ port-id 1)))
             (set-field! outlet-y wire (ycoord yblocks))])
      (send wire update (send (send this get-admin) get-editor)))
    (define/public (adjust-all-wire-location x y)
      (define (helper port-list in-or-out)
        (for ([(p i) (in-indexed port-list)])
          (for ([w (in-list (node-port-wires p))])
            (send this adjust-wire-location x y w i in-or-out))))
      (helper inlets 'inlet)
      (helper outlets 'outlet))
    (define/public (bring-all-wire-to-front pb)
      (define (helper port-list)
        (for ([(p i) (in-indexed port-list)])
          (for ([w (in-list (node-port-wires p))])
            (send pb set-before w #f))))
      (helper inlets)
      (helper outlets))
    (define (set-highlight port-list l)
      (for ([(p i) (in-indexed port-list)])
        (set-node-port-highlight! p (equal? i l))))
    (define/public (refresh-inlet) (send (send this get-admin) needs-update this 0 0 (* block-size (+ xblocks 2)) (* block-size 2)))
    (define/public (refresh-outlet) (send (send this get-admin) needs-update this 0 (* block-size (- yblocks 1)) (* block-size (+ xblocks 2)) (* block-size 2)))
    (define/public (set-highlight-inlet l)
      (set-highlight inlets l)
      (refresh-inlet))
    (define/public (set-highlight-outlet l)
      (set-highlight outlets l)
      (refresh-outlet))
    (define/public (handle-port-event inlet-action outlet-action x y event [other-action (lambda () '())])
      (define (xcoord xb) (+ x node-margin (* xb block-size)))
      (define (ycoord yb) (+ y node-margin (* yb block-size)))
      (define ex (send event get-x))
      (define ey (send event get-y))
      (define mx (- ex x node-margin))
      (define my (- ey y node-margin))
      (define (handle-port-event port-list action)
        (let ([xb (exact-round (/ mx block-size))])
          (when (within-range-around mx (* xb block-size) port-mouse-radius)
            (letrec [(h (lambda (cx l)
                          (when (not (null? l))
                            (cond [(equal? cx xb) (action (- cx 1) (car l)) #t]
                                  [(< cx (- xblocks 1)) (h (+ cx 1) (cdr l))]))))]
              (h 1 port-list)))))
      (cond
        ;; Handle [...] case (number of ports > xblocks - 2)\
        [(within-range-around my 0 port-mouse-radius)
         (when (void? (handle-port-event inlets inlet-action)) (set-highlight-inlet #f))]
        [(within-range-around my (* block-size yblocks) port-mouse-radius)
         (when (void? (handle-port-event outlets outlet-action)) (set-highlight-outlet #f))]
        [#t (set-highlight-inlet #f)
         (set-highlight-outlet #f)
         (other-action)]))
    (define/override (on-event dc
                                x
                                y
                                editorx
                                editory
                                event)
      (define (xcoord xb) (+ editorx node-margin (* xb block-size)))
      (define (ycoord yb) (+ editory node-margin (* yb block-size)))
      (define (port-event-handler highlight-handler wire-initializer ey)
        (lambda (port-id port)
          (cond [(send event moving?) (highlight-handler port-id)]
                [(send event button-down?) (begin
                                             (highlight-handler port-id)
                                             (let ([ex (xcoord (+ port-id 1))])
                                               (let ([nw (new wire-snip% [outlet-x ex] [outlet-y ey] [inlet-x ex] [inlet-y ey])]
                                                     [ed (send (send this get-admin) get-editor)])
                                                 (wire-initializer nw port)
                                                 (send ed insert nw)
                                                 (set-field! incomplete-wire ed nw)
                                                 (set-node-port-wires! port (cons nw (node-port-wires port)))
                                                 (send ed set-before nw #f))))])))
      (handle-port-event (port-event-handler (lambda (l) (send this set-highlight-inlet l))
                                             (lambda (nw port)
                                                                   (set-field! inlet-port nw port)
                                                                   (set-field! inlet-node nw this)) (ycoord 0))
                         (port-event-handler (lambda (l) (send this set-highlight-outlet l))
                                             (lambda (nw port)
                                                                   (set-field! outlet-port nw port)
                                                                   (set-field! outlet-node nw this)) (ycoord yblocks))
                         x y event (lambda () (super on-event dc x y editorx editory event))))
    ))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [inlets (map new-node-port '(signal signal event))] [outlets (map new-node-port '(signal signal event))]))
(send pb insert (new node-snip% [xblocks 5] [yblocks 4]))
(send c set-editor pb)
(send f show #t)
