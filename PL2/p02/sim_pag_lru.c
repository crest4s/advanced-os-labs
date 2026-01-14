/*
    sim_pag_lru.c
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

    // Guardar timestamp LRU
    S->tdp[pagina].timestamp = S->reloj;

    // Incrementar reloj
    S->reloj++;

    // Detectar desbordamiento
    if (S->reloj == 0)
        printf("¡ADVERTENCIA! El reloj se ha desbordado y volvió a 0\n");
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
    int m, p, victima = -1;
    unsigned min_ts = (unsigned)-1;

    for (m = 0; m < S->nummarcos; m++)
    {
        p = S->tdm[m].pagina;
        if (p != -1) {
            if (S->tdp[p].timestamp < min_ts) {
                min_ts = S->tdp[p].timestamp;
                victima = p;
            }
        }
    }

    if (S->detallado)
        printf("@ Eligiendo (LRU) P%d como víctima (timestamp %u)\n",
               victima, min_ts);

    return victima;
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

    // Enlazar marco → página
    S->tdm[marco].pagina = pagina;

    // Enlazar página → marco
    S->tdp[pagina].marco = marco;
    S->tdp[pagina].presente = 1;
    S->tdp[pagina].modificada = 0;
}

// Funciones que muestran resultados

void mostrar_tabla_de_paginas (ssistema * S)
{
    int p;

    printf("%10s %10s %10s %12s %12s %12s\n",
           "PÁGINA", "Presente", "Marco",
           "Modificada", "timestamp", "reloj-tstamp");

    for (p = 0; p < S->numpags; p++)
    {
        if (S->tdp[p].presente)
        {
            printf("%8d   %8d   %8d   %12d   %12u   %12u\n",
                   p,
                   S->tdp[p].presente,
                   S->tdp[p].marco,
                   S->tdp[p].modificada,
                   S->tdp[p].timestamp,
                   S->reloj - S->tdp[p].timestamp);
        }
        else
        {
            // Página no presente: no hay marco, ni timestamp, ni reloj-tstamp
            printf("%8d   %8d   %8s   %12s   %12s   %12s\n",
                   p,
                   S->tdp[p].presente,
                   "-",
                   "-",
                   "-",
                   "-");
        }
    }
}
void mostrar_tabla_de_marcos (ssistema * S)
{
    int p, m;

    printf ("%10s %10s %10s   %s\n",
            "MARCO", "Página", "Presente", "Modificada");

    for (m=0; m<S->nummarcos; m++)
    {
        p = S->tdm[m].pagina;

        if (p==-1)
            printf ("%8d   %8s   %6s     %6s\n", m, "-", "-", "-");
        else if (S->tdp[p].presente)
            printf ("%8d   %8d   %6d     %6d\n", m, p,
                    S->tdp[p].presente, S->tdp[p].modificada);
        else
            printf ("%8d   %8d   %6d     %6s   ¡ERROR!\n", m, p,
                    S->tdp[p].presente, "-");
    }
}

void mostrar_informe_reemplazo (ssistema * S)
{
    int m, p;
    unsigned min = (unsigned)-1;
    unsigned max = 0;

    for (m = 0; m < S->nummarcos; m++) {
        p = S->tdm[m].pagina;
        if (p != -1) {
            if (S->tdp[p].timestamp < min) min = S->tdp[p].timestamp;
            if (S->tdp[p].timestamp > max) max = S->tdp[p].timestamp;
        }
    }

    printf("Reemplazo LRU\n");
    printf("Reloj actual: %u\n", S->reloj);
    printf("Timestamp mínimo: %u\n", min);
    printf("Timestamp máximo: %u\n", max);
}


