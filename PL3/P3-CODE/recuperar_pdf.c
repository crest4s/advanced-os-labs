/*
    Fichero: recuperar_pdf.c
    Autor: Práctica 3 - Fase 3
    
    Programa para recuperar el fichero PDF borrado (_DS.pdf)
    que se encuentra en los clusters marcados como erróneos
    a partir del cluster 7.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "fatsoa.h"

#define IMAGE_FILE "fatsoa.fs"
#define OUTPUT_FILE "_DS.pdf"
#define DELETED_FILE_CLUSTER 7  // Cluster de inicio del fichero borrado

int main(int argc, char *argv[])
{
    int fd_in, fd_out;
    struct BS_Structure bs_data;
    uint32_t fat_begin_offset;
    uint32_t cluster_size;
    uint32_t current_cluster = DELETED_FILE_CLUSTER;
    uint32_t fat_entry;
    char *buffer;
    ssize_t bytes_read, bytes_written;
    uint32_t total_bytes = 0;
    
    // Tamaño del fichero obtenido del directorio raíz (offset 0x36000)
    // El fichero tiene un tamaño específico que se puede ver en el volcado
    // Según el enunciado, debemos obtener este valor del directorio
    // Para este ejemplo, lo calculamos leyendo la entrada del directorio
    uint32_t file_size;
    struct DIR_Structure dir_entry;
    
    printf("=== Programa de recuperación de fichero borrado ===\n");
    printf("Imagen: %s\n", IMAGE_FILE);
    printf("Fichero a recuperar: %s\n", OUTPUT_FILE);
    printf("Cluster inicial: %d\n\n", DELETED_FILE_CLUSTER);
    
    // Abrir imagen del disco
    fd_in = open(IMAGE_FILE, O_RDONLY);
    if (fd_in == -1)
    {
        printf("Error: No se puede abrir la imagen %s: %s\n", IMAGE_FILE, strerror(errno));
        return 1;
    }
    
    // Leer el Boot Sector
    bytes_read = read(fd_in, &bs_data, sizeof(struct BS_Structure));
    if (bytes_read != sizeof(struct BS_Structure))
    {
        printf("Error: No se pudo leer el Boot Sector\n");
        close(fd_in);
        return 1;
    }
    
    // Calcular parámetros del sistema de ficheros
    fat_begin_offset = bs_data.reservedSectorCount * bs_data.bytesPerSector;
    cluster_size = bs_data.bytesPerSector * bs_data.sectorPerCluster;
    
    printf("Bytes por sector: %d\n", bs_data.bytesPerSector);
    printf("Sectores por cluster: %d\n", bs_data.sectorPerCluster);
    printf("Tamaño del cluster: %d bytes\n", cluster_size);
    printf("Offset de la FAT: 0x%X\n\n", fat_begin_offset);
    
    // Leer la entrada del directorio raíz para obtener el tamaño del fichero
    // El directorio raíz está en el cluster 2
    uint32_t root_offset = ((2 - 2) * bs_data.bytesPerSector * bs_data.sectorPerCluster) +
                           (bs_data.bytesPerSector * bs_data.reservedSectorCount) +
                           (bs_data.numberofFATs * bs_data.FATsize_F32 * bs_data.bytesPerSector);
    
    // Buscar la entrada del fichero borrado en el directorio raíz
    lseek(fd_in, root_offset, SEEK_SET);
    int found = 0;
    for (int i = 0; i < 16 && !found; i++)
    {
        bytes_read = read(fd_in, &dir_entry, sizeof(struct DIR_Structure));
        if (bytes_read != sizeof(struct DIR_Structure))
        {
            break;
        }
        
        // Buscar entrada borrada (primer byte = 0xE5) que comience en cluster 7
        if (dir_entry.DIR_name[0] == 0xE5)
        {
            uint32_t cluster = (dir_entry.firstClusterHI << 16) + dir_entry.firstClusterLO;
            if (cluster == DELETED_FILE_CLUSTER)
            {
                file_size = dir_entry.fileSize;
                found = 1;
                printf("Entrada de fichero borrado encontrada:\n");
                printf("  Tamaño: %u bytes (0x%X)\n", file_size, file_size);
                printf("  Cluster inicial: %u\n", cluster);
            }
        }
    }
    
    if (!found)
    {
        printf("ADVERTENCIA: No se encontró la entrada del fichero borrado\n");
        printf("Se extraerán todos los clusters marcados como erróneos desde el cluster 7\n");
        // Establecer un tamaño máximo razonable
        file_size = 0xFFFFFFFF; // Se irá ajustando según los clusters encontrados
    }
    
    // Abrir fichero de salida
    fd_out = open(OUTPUT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out == -1)
    {
        printf("Error: No se puede crear el fichero %s: %s\n", OUTPUT_FILE, strerror(errno));
        close(fd_in);
        return 1;
    }
    
    // Reservar buffer del tamaño de un cluster
    buffer = (char *)malloc(cluster_size);
    if (buffer == NULL)
    {
        printf("Error: No se pudo reservar memoria\n");
        close(fd_in);
        close(fd_out);
        return 1;
    }
    
    printf("\nExtrayendo clusters marcados como erróneos (0x0FFFFFF7)...\n");
    
    // Recorrer los clusters marcados como erróneos
    uint32_t bytes_remaining = file_size;
    uint32_t cluster_count = 0;
    
    do
    {
        // Leer entrada de la FAT para el cluster actual
        lseek(fd_in, fat_begin_offset + (current_cluster << 2), SEEK_SET);
        bytes_read = read(fd_in, &fat_entry, sizeof(fat_entry));
        
        if (bytes_read != sizeof(fat_entry))
        {
            printf("Error leyendo la FAT\n");
            break;
        }
        
        // Verificar si el cluster está marcado como erróneo (0x0FFFFFF7)
        if ((fat_entry & 0x0FFFFFFF) != 0x0FFFFFF7)
        {
            // Si no está marcado como erróneo, hemos terminado
            printf("Cluster %d no está marcado como erróneo (FAT: 0x%08X)\n", 
                   current_cluster, fat_entry);
            break;
        }
        
        printf("Cluster %d - FAT: 0x%08X (erróneo) - ", current_cluster, fat_entry);
        
        // Calcular offset del cluster
        uint32_t cluster_offset = ((current_cluster - 2) * bs_data.bytesPerSector * bs_data.sectorPerCluster) +
                                  (bs_data.bytesPerSector * bs_data.reservedSectorCount) +
                                  (bs_data.numberofFATs * bs_data.FATsize_F32 * bs_data.bytesPerSector);
        
        // Leer el cluster
        lseek(fd_in, cluster_offset, SEEK_SET);
        
        // Determinar cuántos bytes leer de este cluster
        uint32_t bytes_to_read = cluster_size;
        if (found && bytes_remaining < cluster_size)
        {
            bytes_to_read = bytes_remaining;
        }
        
        bytes_read = read(fd_in, buffer, bytes_to_read);
        if (bytes_read != bytes_to_read)
        {
            printf("Error leyendo cluster %d\n", current_cluster);
            break;
        }
        
        // Escribir al fichero de salida
        bytes_written = write(fd_out, buffer, bytes_read);
        if (bytes_written != bytes_read)
        {
            printf("Error escribiendo al fichero de salida\n");
            break;
        }
        
        total_bytes += bytes_written;
        cluster_count++;
        printf("%u bytes escritos (total: %u)\n", (uint32_t)bytes_written, total_bytes);
        
        if (found)
        {
            bytes_remaining -= bytes_written;
            if (bytes_remaining == 0)
            {
                break;
            }
        }
        
        // Pasar al siguiente cluster consecutivo
        current_cluster++;
        
        // Protección: si llevamos demasiados clusters, parar
        if (cluster_count > 1000)
        {
            printf("ADVERTENCIA: Se han procesado más de 1000 clusters. Deteniendo.\n");
            break;
        }
        
    } while (1);
    
    // Liberar recursos
    free(buffer);
    close(fd_in);
    close(fd_out);
    
    printf("\n=== Recuperación completada ===\n");
    printf("Fichero generado: %s\n", OUTPUT_FILE);
    printf("Total de bytes escritos: %u (0x%X)\n", total_bytes, total_bytes);
    printf("Clusters procesados: %u\n", cluster_count);
    
    if (found && total_bytes == file_size)
    {
        printf("El tamaño coincide con el esperado.\n");
    }
    
    return 0;
}
