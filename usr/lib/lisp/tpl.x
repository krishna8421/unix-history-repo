(File tpl.l)
(tpl-*break lambda quote list tpl-break-function cond let)
(tpl-yorn lambda eq tyi let patom progn msg drain)
(tpl-command-reset lambda *throw tpl-throw)
(tpl-command-his-rec lambda terpr car tpl-history-form-print patom progn msg let cdr |1-| tpl-command-his-rec >& > and cond If)
(tpl-command-history lambda tpl-command-his-rec setq memq cond If let)
(tpl-command-prt lambda *throw tpl-throw)
(tpl-next-user-in-history lambda)
(tpl-redo-by-count lambda cdr |1-| car list quote *throw tpl-throw terpr patom progn msg cond If null >& > not or tpl-next-user-in-history do)
(tpl-redo-by-car lambda cdr list quote *throw tpl-throw substring equal cadar setq cdar dtpr and null tpl-next-user-in-history do pntlen get_pname let* terpr patom progn msg symbolp not cadr eq cond If car let)
(tpl-command-redo lambda tpl-redo-by-car terpr patom progn msg lessp not - <& < car fixp tpl-redo-by-count null cond If cdr setq)
(tpl-command-ret lambda terpr patom progn msg cadr eval list quote *throw tpl-throw cond If)
(tpl-command-pop lambda *throw tpl-throw terpr patom progn msg eq =& = cond If)
(tpl-err-tpl-fcn lambda quote cons tpl-break-function)
(tpl-break-function lambda do-one-transaction *catch tpl-catch return dtpr list *throw tpl-throw concat caddr errdesc-contp or cddr cadr terpr print null cdr setq liszt-internal-do mapc cdddr errdesc-descr patom progn msg car eq cond If cons ncons quote |1+| most-recent-given do)
(tpl-zoom lambda terpr patom progn msg tpl-printframelist)
(tpl-printframelist lambda car caddr evalframe-expr print let |1-| cdr tpl-printframelist not length eq =& = terpr patom progn msg null cond If)
(tpl-getframelist lambda cons cdr car null list do cadr evalframe-pdl evalframe setq cond If let)
(tpl-gentrace lambda cdr eq cons not memq and dtpr car caddr evalframe-expr null cond If do nreverse setq tpl-getframelist let)
(tpl-update-stack lambda setq terpr patom progn msg tpl-gentrace tpl-yorn cond If)
(add-to-res-history lambda cdr cons setq)
(add-to-given-history lambda |1+| car eq not cond If cons setq)
(most-recent-given lambda car)
(tpl-command-pp lambda terpr car caddr evalframe-expr pp-form)
(tpl-command-ev lambda setq car cadddr evalframe-bind eval prog1 null terpr patom progn msg symbolp not cond If cadr let)
(tpl-command-up lambda |1-| car cons eq =& = do cdr >& > cadr fixp and cond If let setq)
(tpl-command-down lambda tpl-command-zoom memq |1-| car cons eq =& = do cdr >& > cadr fixp and cond If let setq)
(tpl-command-zoom lambda tpl-zoom setq tpl-update-stack)
(tpl-command-debug lambda setq terpr patom progn msg *rset memq cond If)
(tpl-command-state lambda terpr patom progn msg)
(tpl-command-trace lambda quote apply cdr setq)
(tpl-command-load lambda terpr patom progn msg car load null liszt-internal-do mapc cond If cdr setq)
(tpl-command-help lambda caddr let liszt-internal-do mapc car cdddar return memq dtpr eq caar symbolp and or terpr patom progn msg null cadr do cdr cond If setq)
(process-fcn lambda cdr cadar tpl-funcall return memq dtpr eq caar symbolp and or cond If terpr patom progn msg null do setq car let)
(ntpl-print lambda print)
(tpl-history-form-print lambda null setq liszt-internal-do mapc patom cdr print car quote eq cond If)
(ntpl-read lambda read untyi cdr nreverse readlist errset quote cons car or list cond If tyi tyipeek setq eq not and do let)
(do-one-transaction lambda ntpl-print add-to-res-history not process-fcn cdr tpl-eval add-to-given-history tpl-history-form-print exit terpr progn msg quote status eq ntpl-read errset car setq cond If patom let)
(tpl lambda tpl-break-function *catch tpl-catch do cxr getd putd quote setq)
