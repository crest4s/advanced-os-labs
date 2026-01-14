/*
    sim_pag_fifo2opc
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sim_paginacion.h"

// Función que inicia las tablas

void iniciar_tablas (ssistema * S)
{
    int i;

    // Páginas a cero
    memset (S->tdp, 0, sizeof(spagina)*S->numpags);

    // Pila LRU vacía
    S->lru = -1;

    // Tiempo LRU(t) a cero
    S->reloj = 0;

    // Lista circular de marcos libres
    for (i=0; i<S->nummarcos-1; i++)
    {
        S->tdm[i].pagina = -1;
        S->tdm[i].sig = i+1;
    }

    S->tdm[i].pagina = -1;  // Ahora i == nummarcos-1
    S->tdm[i].sig = 0;      // Cerrar lista circular
    S->listalibres = i;     // Apuntar al último

    // Lista circular de marcos ocupados vacía
    S->listaocupados = -1;
}

// Funciones que simulan el hardware de la MMU

unsigned sim_mmu (ssistema * S, unsigned dir_virtual, char op)
{
    unsigned pagina, desplazamiento, marco, dir_fisica;

    // 1) Calcular número de página y offset
    pagina = dir_virtual / S->tampag;
    desplazamiento = dir_virtual % S->tampag;

    // 2) Comprobar que la dirección es legal
    if (pagina < 0 || pagina >= S->numpags) {
        S->numrefsilegales++;
        return ~0U;  // Dirección física FF...F
    }

    // 3) Si la página no está en memoria → fallo de página
    if (!S->tdp[pagina].presente)
        tratar_fallo_de_pagina(S, dir_virtual);

    // 4) Traducir a dirección física
    marco = S->tdp[pagina].marco;
    dir_fisica = marco * S->tampag + desplazamiento;

    // 5) Marcar la referencia
    referenciar_pagina(S, pagina, op);

    // 6) Modo detallado
    if (S->detallado)
        printf("\t%c %u==P%d (M%d)+%d\n",
               op, dir_virtual, pagina, marco, desplazamiento);

    return dir_fisica;
}

void referenciar_pagina (ssistema * S, int pagina, char op)
{
    if (op=='L')
        S->numrefslectura ++;
    else if (op=='E') {
        S->tdp[pagina].modificada = 1;
        S->numrefsescritura ++;
    }

    // Segunda oportunidad: bit de referencia
    S->tdp[pagina].referenciada = 1;
}

// Funciones que simulan el sistema operativo

void tratar_fallo_de_pagina (ssistema * S, unsigned dir_virtual)
{
    int pagina, victima, marco, ult;

    // 1) Calcular página
    pagina = dir_virtual / S->tampag;
    S->numfallospag++;

    if (S->detallado)
        printf("@ FALLO DE PÁGINA en P%d!\n", pagina);

    // 2) Si hay marcos libres
    if (S->listalibres != -1) {

        ult = S->listalibres;        // Último de la lista circular
        marco = S->tdm[ult].sig;     // El siguiente es el primero libre

        // Si solo queda uno
        if (marco == ult)
            S->listalibres = -1;
        else
            S->tdm[ult].sig = S->tdm[marco].sig;

        ocupar_marco_libre(S, marco, pagina);
    }
    else {
        // 3) No hay marcos libres → reemplazo
        victima = elegir_pagina_para_reemplazo(S);
        reemplazar_pagina(S, victima, pagina);
    }
}

int elegir_pagina_para_reemplazo (ssistema * S)
{
    int ultimo = S->listaocupados;
    int marco = S->tdm[ultimo].sig;  // primer marco FIFO

    while (1)
    {
        int p = S->tdm[marco].pagina;

        if (S->tdp[p].referenciada == 0) {
            // Esta página NO tiene segunda oportunidad → víctima
            if (S->detallado)
                printf("@ Víctima (FIFO 2OP): P%d en M%d\n", p, marco);

            // mover marco al final (como FIFO normal)
            S->listaocupados = marco;

            return p;
        }

        // Tenía bit de referencia → darle una segunda oportunidad
        if (S->detallado)
            printf("@ P%d en M%d tenía bit R=1 → se lo ponemos a 0\n",
                   p, marco);

        // borrar bit de referencia
        S->tdp[p].referenciada = 0;

        // avanzar al siguiente marco
        marco = S->tdm[marco].sig;
    }
}

void reemplazar_pagina (ssistema * S, int victima, int nueva)
{
    int marco;

    marco = S->tdp[victima].marco;

    if (S->tdp[victima].modificada)
    {
        if (S->detallado)
            printf ("@ Volcando P%d modificada a disco para "
                    "reemplazarla\n", victima);

        S->numescrpag ++;
    }

    if (S->detallado)
        printf ("@ Reemplazando víctima P%d por P%d en M%d\n",
                victima, nueva, marco);

    S->tdp[victima].presente = 0;

    S->tdp[nueva].presente = 1;
    S->tdp[nueva].marco = marco;
    S->tdp[nueva].modificada = 0;

    S->tdm[marco].pagina = nueva;
}

void ocupar_marco_libre (ssistema * S, int marco, int pagina)
{
    if (S->detallado)
        printf("@ Alojando P%d en M%d\n", pagina, marco);

    // Enlazar marco ↔ página
    S->tdm[marco].pagina = pagina;
    S->tdp[pagina].marco = marco;
    S->tdp[pagina].presente = 1;
    S->tdp[pagina].modificada = 0;

    // -----------------------------
    // INSERTAR MARCO EN LA LISTA FIFO
    // -----------------------------
    if (S->listaocupados == -1) {
        // Lista vacía: marco se apunta a sí mismo
        S->listaocupados = marco;
        S->tdm[marco].sig = marco;
    }
    else {
        int ultimo = S->listaocupados;
        int primero = S->tdm[ultimo].sig;

        // nuevo marco → primero
        S->tdm[marco].sig = primero;
        // último → nuevo marco
        S->tdm[ultimo].sig = marco;
        // y nuevo marco se convierte en el último
        S->listaocupados = marco;
    }
}

// Funciones que muestran resultados

void mostrar_tabla_de_paginas (ssistema * S)
{
    int p;

    printf("%10s %10s %10s %12s %12s\n",
       "PÁGINA", "Presente", "Marco", "Modif", "Ref");

    for (p=0; p<S->numpags; p++)
        if (S->tdp[p].presente)
            printf("%8d %10d %10d %12d %12d\n",
                p,
                S->tdp[p].presente,
                S->tdp[p].marco,
                S->tdp[p].modificada,
                S->tdp[p].referenciada);
        else
            printf("%8d %10d %10s %12s %12s\n",
                p,
                S->tdp[p].presente,
                "-",
                "-",
                "-");
}

void mostrar_tabla_de_marcos (ssistema * S)
{
    int p, m;

    // Cabecera de la tabla
    printf("%10s %10s %10s %12s %12s\n",
           "MARCO", "Página", "Presente", "Modif", "Ref");

    for (m = 0; m < S->nummarcos; m++)
    {
        p = S->tdm[m].pagina;

        if (p == -1) {
            // Marco libre: no hay página asociada
            printf("%8d %10s %10s %12s %12s\n",
                   m, "-", "-", "-", "-");
        }
        else if (S->tdp[p].presente) {
            // Marco ocupado con página presente
            printf("%8d %10d %10d %12d %12d\n",
                   m,
                   p,
                   S->tdp[p].presente,
                   S->tdp[p].modificada,
                   S->tdp[p].referenciada);
        }
        else {
            // Marco apunta a página NO presente (no debería ocurrir)
            printf("%8d %10d %10d %12s %12s   ¡ERROR!\n",
                   m,
                   p,
                   S->tdp[p].presente,
                   "-",
                   "-");
        }
    }
}

void mostrar_informe_reemplazo (ssistema * S)
{
    printf("Reemplazo FIFO con segunda oportunidad\n");
    printf("Marcos ocupados y bit R:\n");

    if (S->listaocupados == -1) {
        printf("(vacío)\n");
        return;
    }

    int ultimo = S->listaocupados;
    int m = S->tdm[ultimo].sig;   // ← AQUÍ SE INICIALIZA m

    do {
        int p = S->tdm[m].pagina; // ← AQUÍ SE INICIALIZA p
        printf("M%d → P%d (R=%d)\n",
               m, p, S->tdp[p].referenciada);

        m = S->tdm[m].sig;
    } while (m != S->tdm[ultimo].sig);
}
