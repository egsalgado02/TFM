#include <stdlib.h>
#include "fsm.h"

// fsm_new: crea una FSM con malloc. NO la uso en producción (el embebido
// no quiere heap dinámico para esto), pero la dejo por si acaso.
fsm_t* fsm_new(fsm_trans_t* tt)
{
    fsm_t* this = (fsm_t*) malloc(sizeof(fsm_t));
    fsm_init(this, tt);
    return this;
}

// Inicializa una FSM ya declarada. El estado inicial es el origen
// de la PRIMERA fila de la tabla.
void fsm_init(fsm_t* this, fsm_trans_t* tt)
{
    this->tt            = tt;
    this->current_state = tt[0].orig_state;
}

// El motor. Recorre la tabla buscando una transición válida:
//   1) que el estado origen coincida con el actual
//   2) que la guarda devuelva true
// Si encuentra una, cambia de estado, ejecuta la acción y SALE.
// El final de la tabla está marcado con orig_state = -1.
void fsm_fire(fsm_t* this)
{
    fsm_trans_t* t;
    for (t = this->tt; t->orig_state >= 0; ++t) {
        if ((this->current_state == t->orig_state) && t->in(this)) {
            this->current_state = t->dest_state;
            if (t->out)
                t->out(this);
            break;   // solo UNA transición por fire
        }
    }
}
