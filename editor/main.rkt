#lang racket/gui
(define (is-signal-port? p) (eq? p 'signal))
(define (is-event-port? p) (eq? p 'event))
(define block-size 20)
(define round-size (* block-size 0.25))
(define port-radius (* block-size 0.1))
(define border-width (* block-size 0.05))
(define border-color (make-object color% 100 100 100))
(define grid-width (* block-size 0.05))
(define grid-color (make-object color% 50 50 50))
(define background-color (make-object color% 30 30 30))
(define signal-port-color (make-object color% 0 171 173))
(define event-port-color (make-object color% 105 134 206))

(define f (new frame% [label "Simple Edit"]
               [width 1000]
               [height 1000]))
(define c (new editor-canvas% [parent f]))
(define graph-board%
  (class pasteboard%
    ;; rest of the chess-board% definition remains unchanged...
    (super-new)
    (define/override (on-paint before? dc . other)
      (when before?
        (draw-background dc)))
  (define (draw-background dc)
    (define-values (dc-width dc-height) (send dc get-size))
    (define-values (dx dy) (send this editor-location-to-dc-location 0 0))
    (define xm (+ dx dc-width))
    (define ym (+ dy dc-height))
    (send dc clear)
    (send dc set-smoothing 'smoothed)
    (send dc set-background background-color)
    (send dc set-pen grid-color grid-width 'solid)
    (for ([i (in-range (/ dc-height block-size))])
      (let ([y (+ dy (* block-size i))])
        (send dc draw-line dx y xm y)))
    (for ([i (in-range (/ dc-width block-size))])
      (let ([x (+ dx (* block-size i))])
        (send dc draw-line x dx x ym))))
    (define/augment (after-select snip on?)
      (when on?
        (send this set-before snip #f)
        ;; Rest of the after-select definition remains unchanged...
        ))
    (define/augment (after-interactive-move event)
      (define sx (box 0))
      (define sy (box 0))
      (define s1 (send this find-next-selected-snip #f))
      (send this get-snip-location s1 sx sy)
      (let ([dx (- (* (round (/ (unbox sx) block-size)) block-size) (unbox sx))]
            [dy (- (* (round (/ (unbox sy) block-size)) block-size) (unbox sy))])
        (letrec ([h (lambda (s)
                      (when s (send this get-snip-location s sx sy)
                            (send this move-to s (+ (unbox sx) dx) (+ (unbox sy) dy))
                            (h (send this find-next-selected-snip s))))])
          (h s1))))
    (define/override (on-double-click snip event)
      '())
    ))
(define pb (new graph-board%))
(define node-snip-class%
  (class snip-class%
    (inherit set-classname)
    (super-new)
    ))
(define node-snip-class (new node-snip-class%))

(define node-snip%
  (class snip%
      (inherit set-snipclass
               get-flags set-flags
               get-admin)
      (init-field [xblocks 10]
                  [yblocks 1]
                  [extended #f]
                  [inlets '(signal)]
                  [outlets '(event)]
                  [inlets-wire '(())]
                  [outlets-wire '(())])

    (super-new)
    (set-snipclass node-snip-class)
    (send (get-the-snip-class-list) add node-snip-class)
    (set-flags (cons 'handles-all-mouse-events (get-flags)))

    (define/override (get-extent dc x y
                                 [w #f]
                                 [h #f]
                                 [descent #f]
                                 [space #f]
                                 [lspace #f]
                                 [rspace #f])
      (define (maybe-set-box! b v) (when b (set-box! b v)))
      (maybe-set-box! w (* (+ xblocks 2) block-size))
      (maybe-set-box! h (* (+ yblocks 2) block-size))
      (maybe-set-box! descent block-size)
      (maybe-set-box! space block-size)
      (maybe-set-box! lspace block-size)
      (maybe-set-box! rspace block-size))

    (define/override (draw dc x y left top right bottom dx dy draw-caret)
      (define (xcoord xb) (+ x (* xb block-size)))
      (define (ycoord yb) (+ y (* yb block-size)))
      (define (draw-port num type w y)
        (define rad port-radius)
        (define size (* rad 2))
        (define c (cond ((is-signal-port? type) signal-port-color)
                        ((is-event-port? type) event-port-color)
                        ('t (raise "Unknown port type"))))
        (send dc set-pen c border-width 'solid)
        (send dc set-brush (if (null? w) background-color c) 'solid)
        (send dc draw-ellipse (- (xcoord (+ num 2)) rad) (- y rad) size size))
      (send dc set-pen border-color border-width 'solid)
      (send dc set-brush background-color 'solid)
      (send dc draw-rounded-rectangle (xcoord 1) (ycoord 1) (* xblocks block-size) (* yblocks block-size) round-size)
      (for ([(p i) (in-indexed inlets)]
            [wires (in-list inlets-wire)])
        (draw-port i p wires (ycoord 1)))
      (for ([(p i) (in-indexed outlets)]
            [wires (in-list outlets-wire)])
        (draw-port i p wires (ycoord (+ yblocks 1)))))
    (define/override (copy)
      (new node-snip% [xblocks yblocks]))
    (define/override (on-event dc
                                x
                                y
                                editorx
                                editory
                                event)
      (when (send event button-up?) (println event)))
    ))
(send pb insert (new node-snip%))
(send pb insert (new node-snip% [xblocks 5] [yblocks 4]))
(send c set-editor pb)
(send f show #t)
