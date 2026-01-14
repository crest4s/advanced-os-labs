/*
    sim_pag_nfu.c — Ampliación Práctica 2
    Algoritmo de reemplazo NFU (Not Frequently Used)
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sim_paginacion.h"

#define NUM_TICKS 4      // Puedes cambiarlo si lo pide el enunciado
#define CNT_MAX   20
#define CNT_MIN    0

// ---------------------------------------------------------
//  INICIALIZAR TABLAS
// ---------------------------------------------------------
void iniciar_tablas (ssistema * S)
{
    int i;

    // Limpiar tabla de páginas
    memset(S->tdp, 0, sizeof(spagina)*S->numpags);

    // Inicializar contadores NFU
    for (i = 0; i < S->numpags; i++)
        S->contador[i] = 0;

    // Reloj NFU
    S->reloj = 0;

    // Lista circular de marcos libres
    for (i = 0; i < S->nummarcos - 1; i++)
    {
        S->tdm[i].pagina = -1;
        S->tdm[i].sig = i + 1;
    }
    S->tdm[i].pagina = -1;
    S->tdm[i].sig = 0;
    S->listalibres = i;

    // Lista circular de marcos ocupados
    S->listaocupados = -1;
}



// ---------------------------------------------------------
//  SIMULACIÓN DE LA MMU
// ---------------------------------------------------------
unsigned sim_mmu (ssistema * S, unsigned dir_virtual, char op)
{
    unsigned pagina, desplazamiento, marco, dir_fisica;

    pagina = dir_virtual / S->tampag;
    desplazamiento = dir_virtual % S->tampag;

    if (pagina >= S->numpags)
    {
        S->numrefsilegales++;
        return ~0U;
    }

    if (!S->tdp[pagina].presente)
        tratar_fallo_de_pagina(S, dir_virtual);

    marco = S->tdp[pagina].marco;
    dir_fisica = marco * S->tampag + desplazamiento;

    referenciar_pagina(S, pagina, op);

    if (S->detallado)
        printf("\t%c %u == P%d (M%d) + %d\n",
               op, dir_virtual, pagina, marco, desplazamiento);

    return dir_fisica;
}



// ---------------------------------------------------------
//  REFERENCIAR PÁGINA (NFU)
// ---------------------------------------------------------
void referenciar_pagina (ssistema * S, int pagina, char op)
{
    if (op == 'L')
        S->numrefslectura++;
    else if (op == 'E')
    {
        S->numrefsescritura++;
        S->tdp[pagina].modificada = 1;
    }

    // Activar bit de referencia
    S->tdp[pagina].referenciada = 1;

    // Incrementar reloj
    S->reloj++;

    // ¿Toca actualización NFU?
    if (S->reloj % NUM_TICKS == 0)
    {
        int p;
        if (S->detallado)
            printf("@ [NFU] Actualizando contadores (tick)\n");

        for (p = 0; p < S->numpags; p++)
        {
            if (!S->tdp[p].presente)
                continue;

            if (S->tdp[p].referenciada)
            {
                if (S->contador[p] < CNT_MAX)
                    S->contador[p]++;

                S->tdp[p].referenciada = 0;
            }
            else
            {
                if (S->contador[p] > CNT_MIN)
                    S->contador[p]--;
            }
        }
    }
}



// ---------------------------------------------------------
//  FALLO DE PÁGINA
// ---------------------------------------------------------
void tratar_fallo_de_pagina (ssistema * S, unsigned dir_virtual)
{
    int pagina = dir_virtual / S->tampag;
    int marco, ult, victima;

    S->numfallospag++;

    if (S->detallado)
        printf("@ ¡FALLO DE PÁGINA en P%d!\n", pagina);

    // Si hay marcos libres
    if (S->listalibres != -1)
    {
        ult = S->listalibres;
        marco = S->tdm[ult].sig;

        if (marco == ult)
            S->listalibres = -1;
        else
            S->tdm[ult].sig = S->tdm[marco].sig;

        ocupar_marco_libre(S, marco, pagina);
    }
    else
    {
        // No hay marcos libres → NFU
        victima = elegir_pagina_para_reemplazo(S);
        reemplazar_pagina(S, victima, pagina);
    }
}



// ---------------------------------------------------------
//  ELEGIR VÍCTIMA — NFU
// ---------------------------------------------------------
int elegir_pagina_para_reemplazo (ssistema * S)
{
    int ultimo = S->listaocupados;
    int m = S->tdm[ultimo].sig;  // primero
    int victima = -1;
    int min = 99999;

    do {
        int p = S->tdm[m].pagina;

        if (S->contador[p] < min)
        {
            min = S->contador[p];
            victima = p;
        }

        m = S->tdm[m].sig;
    } while (m != S->tdm[ultimo].sig);

    if (S->detallado)
        printf("@ NFU elige P%d (contador=%d) como víctima\n", victima, min);

    return victima;
}



// ---------------------------------------------------------
//  OCUPAR MARCO LIBRE
// ---------------------------------------------------------
void ocupar_marco_libre (ssistema * S, int marco, int pagina)
{
    if (S->detallado)
        printf("@ Alojando P%d en M%d\n", pagina, marco);

    S->tdm[marco].pagina = pagina;
    S->tdp[pagina].marco = marco;
    S->tdp[pagina].presente = 1;
    S->tdp[pagina].modificada = 0;

    if (S->listaocupados == -1)
    {
        S->listaocupados = marco;
        S->tdm[marco].sig = marco;
    }
    else
    {
        int ultimo = S->listaocupados;
        int primero = S->tdm[ultimo].sig;

        S->tdm[marco].sig = primero;
        S->tdm[ultimo].sig = marco;
        S->listaocupados = marco;
    }
}



// ---------------------------------------------------------
//  REEMPLAZAR PÁGINA
// ---------------------------------------------------------
void reemplazar_pagina (ssistema * S, int victima, int nueva)
{
    int marco = S->tdp[victima].marco;

    if (S->tdp[victima].modificada)
    {
        if (S->detallado)
            printf("@ Volcando P%d modificada a disco\n", victima);
        S->numescrpag++;
    }

    if (S->detallado)
        printf("@ Reemplazando P%d por P%d en M%d\n",
               victima, nueva, marco);

    S->tdp[victima].presente = 0;

    S->tdp[nueva].presente = 1;
    S->tdp[nueva].marco = marco;
    S->tdp[nueva].modificada = 0;

    S->tdm[marco].pagina = nueva;
}



// ---------------------------------------------------------
//  MOSTRAR TABLAS
// ---------------------------------------------------------
void mostrar_tabla_de_paginas (ssistema * S)
{
    int p;

    printf("%10s %10s %10s %12s %12s %12s\n",
           "PÁGINA", "Presente", "Marco", "Modif", "Ref", "Cnt");

    for (p = 0; p < S->numpags; p++)
    {
        if (S->tdp[p].presente)
            printf("%8d %10d %10d %12d %12d %12d\n",
                   p,
                   S->tdp[p].presente,
                   S->tdp[p].marco,
                   S->tdp[p].modificada,
                   S->tdp[p].referenciada,
                   S->contador[p]);
        else
            printf("%8d %10d %10s %12s %12s %12s\n",
                   p, S->tdp[p].presente, "-", "-", "-", "-");
    }
}



void mostrar_tabla_de_marcos (ssistema * S)
{
    int m, p;

    printf("%10s %10s %10s %12s %12s %12s\n",
           "MARCO", "Página", "Presente", "Modif", "Ref", "Cnt");

    for (m = 0; m < S->nummarcos; m++)
    {
        p = S->tdm[m].pagina;

        if (p == -1)
            printf("%8d %10s %10s %12s %12s %12s\n",
                   m, "-", "-", "-", "-", "-");
        else
            printf("%8d %10d %10d %12d %12d %12d\n",
                   m, p,
                   S->tdp[p].presente,
                   S->tdp[p].modificada,
                   S->tdp[p].referenciada,
                   S->contador[p]);
    }
}



// ---------------------------------------------------------
//  MOSTRAR INFORME DE REEMPLAZO (NFU)
// ---------------------------------------------------------
void mostrar_informe_reemplazo (ssistema * S)
{
    int p, m;
    int min = 99999, max = -1;

    printf("Reemplazo NFU\n");
    printf("Reloj = %u\n", S->reloj);

    for (m = 0; m < S->nummarcos; m++)
    {
        p = S->tdm[m].pagina;
        if (p == -1) continue;

        if (S->contador[p] < min) min = S->contador[p];
        if (S->contador[p] > max) max = S->contador[p];
    }

    printf("Contador mínimo = %d\n", min);
    printf("Contador máximo = %d\n", max);
}