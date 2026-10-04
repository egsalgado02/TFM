#ifndef FSM_H
#define FSM_H

// Motor genérico de máquinas de estado.
// Todas las FSM del proyecto (accel, bme, ina, adc, power, payload)
// usan ESTE motor por dentro. La idea:
//   - cada FSM tiene una "tabla de transiciones" con la forma
//     {estado_origen, guarda, estado_destino, acción}
//   - fsm_fire() recorre la tabla, busca la primera fila cuyo estado
//     coincida y cuya guarda devuelva true, cambia de estado y ejecuta
//     la acción.
// Si quiero ver cómo se usa, mirar accel_fsm.c o bme_fsm.c.

typedef struct fsm_t fsm_t;

// Guarda: devuelve true/false para decir si la transición puede dispararse.
// Acción: lo que se ejecuta justo después de la transición.
typedef int  (*fsm_input_func_t) (fsm_t*);
typedef void (*fsm_output_func_t)(fsm_t*);

// Una fila de la tabla de transiciones.
typedef struct fsm_trans_t {
    int                orig_state;   // -1 marca el final de la tabla
    fsm_input_func_t   in;           // la guarda
    int                dest_state;
    fsm_output_func_t  out;          // la acción
} fsm_trans_t;

// El "objeto" FSM básico. Para extenderlo, basta con poner un fsm_t
// COMO PRIMER CAMPO de tu struct (truco de polimorfismo en C).
// Así puedo hacer (fsm_t*)&mi_accel y el motor genérico ya entiende.
struct fsm_t {
    int           current_state;
    fsm_trans_t*  tt;        // puntero a la tabla de transiciones
};

// Crea una FSM con malloc. En este proyecto NO la uso porque en embebido
// preferimos asignación estática; uso fsm_init() sobre un fsm_t ya declarado.
fsm_t* fsm_new (fsm_trans_t* tt);

// Inicializa una FSM ya existente. Esto es lo que uso normalmente.
void   fsm_init(fsm_t* this, fsm_trans_t* tt);

// El motor: recorre la tabla y ejecuta UNA transición si toca.
// Si quiero dejarla "girar" hasta que se estabilice, llamo varias veces.
void   fsm_fire(fsm_t* this);

#endif
