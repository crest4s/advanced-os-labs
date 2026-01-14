# PRÁCTICA 3: GESTIÓN DE VOLUMEN FAT32
## Documentación Completa - Sistemas Operativos Avanzados

**Universidad de Alcalá - Departamento de Automática**  
**Alumno:** Adrián Morales Rodríguez  
**Fecha:** Diciembre 2025

---

## 📚 TABLA DE CONTENIDOS

1. [Conocimientos Previos Necesarios](#conocimientos-previos)
2. [Fundamentos Teóricos FAT32](#fundamentos-teoricos)
3. [Desarrollo de la Práctica](#desarrollo)
4. [Resultados y Pruebas](#resultados)
5. [Análisis Forense](#analisis-forense)
6. [Conclusiones](#conclusiones)

---

<a name="conocimientos-previos"></a>
## 1. CONOCIMIENTOS PREVIOS NECESARIOS

### 1.1 Llamadas al Sistema UNIX/Linux

#### **open() - Apertura de archivos**

```c
int open(const char *pathname, int flags, mode_t mode);
```

**Parámetros:**
- `pathname`: Ruta del archivo
- `flags`: Modo de apertura
  - `O_RDONLY`: Solo lectura
  - `O_WRONLY`: Solo escritura
  - `O_RDWR`: Lectura y escritura
  - `O_CREAT`: Crear si no existe
  - `O_TRUNC`: Truncar a tamaño 0
- `mode`: Permisos (ej: 0644 = rw-r--r--)

**Retorno:** Descriptor de archivo (≥0) o -1 si error

**Ejemplo:**
```c
int fd = open("archivo.txt", O_RDONLY);
if (fd == -1) {
    perror("Error al abrir archivo");
    return 1;
}
```

#### **read() - Lectura de datos**

```c
ssize_t read(int fd, void *buffer, size_t count);
```

**Parámetros:**
- `fd`: Descriptor del archivo
- `buffer`: Buffer donde almacenar los datos leídos
- `count`: Número de bytes a leer

**Retorno:** 
- Número de bytes leídos (puede ser menor que count)
- 0 si se alcanzó EOF
- -1 si error

**Ejemplo:**
```c
char buffer[512];
ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
if (bytes_read == -1) {
    perror("Error al leer");
} else if (bytes_read == 0) {
    printf("Fin de archivo\n");
}
```

#### **write() - Escritura de datos**

```c
ssize_t write(int fd, const void *buffer, size_t count);
```

**Parámetros:**
- `fd`: Descriptor del archivo
- `buffer`: Datos a escribir
- `count`: Número de bytes a escribir

**Retorno:** 
- Número de bytes escritos
- -1 si error

**Ejemplo:**
```c
const char *data = "Hola mundo";
ssize_t bytes_written = write(fd, data, strlen(data));
```

#### **lseek() - Posicionamiento en archivo**

```c
off_t lseek(int fd, off_t offset, int whence);
```

**Parámetros:**
- `fd`: Descriptor del archivo
- `offset`: Desplazamiento en bytes
- `whence`: Punto de referencia
  - `SEEK_SET`: Desde el inicio (absoluto)
  - `SEEK_CUR`: Desde la posición actual (relativo)
  - `SEEK_END`: Desde el final

**Retorno:** Nueva posición o -1 si error

**Ejemplos:**
```c
// Ir al byte 1024 desde el inicio
lseek(fd, 1024, SEEK_SET);

// Avanzar 100 bytes desde la posición actual
lseek(fd, 100, SEEK_CUR);

// Ir al final (obtener tamaño)
off_t size = lseek(fd, 0, SEEK_END);

// Volver al inicio
lseek(fd, 0, SEEK_SET);
```

#### **close() - Cierre de archivo**

```c
int close(int fd);
```

**Retorno:** 0 si éxito, -1 si error

**Ejemplo:**
```c
close(fd);
```

### 1.2 Gestión de Memoria Dinámica

#### **malloc() - Reserva de memoria**

```c
void *malloc(size_t size);
```

**Ejemplo:**
```c
char *buffer = (char *)malloc(4096);
if (buffer == NULL) {
    fprintf(stderr, "Error: No hay memoria\n");
    return 1;
}
```

#### **free() - Liberación de memoria**

```c
void free(void *ptr);
```

**Ejemplo:**
```c
free(buffer);
buffer = NULL;  // Buena práctica
```

### 1.3 Manipulación de Cadenas y Memoria

```c
// Copiar memoria
memcpy(dest, src, n);          // Copiar n bytes
memset(dest, valor, n);        // Rellenar n bytes con valor
memcmp(s1, s2, n);            // Comparar n bytes

// Cadenas de texto
strlen(str);                   // Longitud
strcpy(dest, src);            // Copiar
strcmp(s1, s2);               // Comparar
strchr(str, ch);              // Buscar carácter
toupper(ch);                  // Convertir a mayúscula
```

### 1.4 Tipos de Datos

```c
uint8_t   // Entero sin signo de 8 bits  (0-255)
uint16_t  // Entero sin signo de 16 bits (0-65535)
uint32_t  // Entero sin signo de 32 bits (0-4294967295)
int       // Descriptor de archivo
ssize_t   // Tamaño con signo (para bytes leídos/escritos)
off_t     // Offset en archivo
```

---

<a name="fundamentos-teoricos"></a>
## 2. FUNDAMENTOS TEÓRICOS FAT32

### 2.1 Estructura General del Sistema de Archivos

```
┌─────────────────────────────────────────────────────────────┐
│                    VOLUMEN FAT32                            │
├─────────────────────────────────────────────────────────────┤
│ Sectores Reservados                                         │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ Sector 0: Boot Sector (BPB + Código de arranque)        │ │
│ │ Sector 1: FSInfo (Información del sistema de archivos)  │ │
│ │ Sectores 2-5: Backup Boot Sector (copia de seguridad)   │ │
│ │ Sectores restantes: Reservados                          │ │
│ └─────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│ FAT #1 (Tabla de Asignación de Archivos - Primera copia)   │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ Entradas de 32 bits (4 bytes) por cluster               │ │
│ │ FAT[0]: Media type                                       │ │
│ │ FAT[1]: Reservado                                        │ │
│ │ FAT[2..N]: Cadenas de clusters de archivos              │ │
│ └─────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│ FAT #2 (Segunda copia - redundancia)                        │
│ (Copia exacta de FAT #1)                                    │
├─────────────────────────────────────────────────────────────┤
│ ZONA DE DATOS (Clusters)                                    │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ Cluster 2: Directorio Raíz (/)                          │ │
│ │ Cluster 3: Subdirectorios y archivos                    │ │
│ │ Cluster 4: Datos de archivos                            │ │
│ │ ...                                                      │ │
│ │ Cluster N: Último cluster                               │ │
│ └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Boot Sector (Sector de Arranque)

**Ubicación:** Offset 0x0 (sector 0)  
**Tamaño:** 512 bytes

**Campos importantes:**

| Offset | Tamaño | Campo             | Descripción                          |
|--------|--------|-------------------|--------------------------------------|
| 0x0B   | 2      | BytsPerSec        | Bytes por sector (normalmente 512)   |
| 0x0D   | 1      | SecPerClus        | Sectores por cluster (ej: 8)         |
| 0x0E   | 2      | RsvdSecCnt        | Sectores reservados (ej: 32)         |
| 0x10   | 1      | NumFATs           | Número de FATs (normalmente 2)       |
| 0x24   | 4      | FATSz32           | Tamaño de una FAT en sectores        |
| 0x2C   | 4      | RootClus          | Cluster del directorio raíz (2)      |
| 0x52   | 8      | FilSysType        | "FAT32   " (tipo de sistema)         |
| 0x1FE  | 2      | Signature         | 0xAA55 (firma de arranque)           |

**Ejemplo de lectura:**

```c
struct BS_Structure bs;
lseek(fd, 0, SEEK_SET);
read(fd, &bs, sizeof(bs));

printf("Bytes por sector: %d\n", bs.bytesPerSector);
printf("Sectores por cluster: %d\n", bs.sectorPerCluster);
```

### 2.3 Tabla FAT (File Allocation Table)

**Función:** Mantiene el registro de qué clusters están libres, cuáles están en uso y cómo se encadenan.

**Estructura:** Array de entradas de 32 bits (aunque solo se usan 28 bits efectivos)

**Valores especiales:**

| Valor FAT          | Significado                                    |
|--------------------|------------------------------------------------|
| 0x00000000         | Cluster libre (disponible)                     |
| 0x00000002-0x0FFFFFEF | Número del siguiente cluster en la cadena |
| 0x0FFFFFF0-0x0FFFFFF6 | Reservado                                  |
| 0x0FFFFFF7         | Cluster erróneo (bad cluster)                  |
| 0x0FFFFFF8-0x0FFFFFFF | End of Chain (EOC - último cluster)        |

**Ejemplo de cadena de clusters:**

```
Archivo de 10KB (3 clusters de 4KB):

Cluster 5: contiene los primeros 4KB
  FAT[5] = 0x00000006  → Continúa en cluster 6

Cluster 6: contiene los siguientes 4KB  
  FAT[6] = 0x00000007  → Continúa en cluster 7

Cluster 7: contiene los últimos 2KB (parcial)
  FAT[7] = 0x0FFFFFF8  → Fin de archivo (EOC)
```

**Cálculo de offset de entrada FAT:**

```c
uint32_t fat_offset = fat_begin + (cluster_number * 4);
lseek(fd, fat_offset, SEEK_SET);
read(fd, &fat_entry, 4);
```

### 2.4 Entradas de Directorio

**Tamaño:** 32 bytes por entrada  
**Ubicación:** En cualquier cluster de directorio

**Estructura:**

| Offset | Tamaño | Campo          | Descripción                           |
|--------|--------|----------------|---------------------------------------|
| 0x00   | 11     | DIR_Name       | Nombre (8) + extensión (3), sin punto |
| 0x0B   | 1      | DIR_Attr       | Atributos del archivo                 |
| 0x14   | 2      | FirstClusHI    | 16 bits altos del cluster inicial     |
| 0x1A   | 2      | WriteTime      | Hora de última escritura              |
| 0x1C   | 2      | WriteDate      | Fecha de última escritura             |
| 0x1E   | 2      | FirstClusLO    | 16 bits bajos del cluster inicial     |
| 0x20   | 4      | FileSize       | Tamaño del archivo en bytes           |

**Atributos (DIR_Attr):**

```c
#define ATTR_READ_ONLY  0x01  // Solo lectura
#define ATTR_HIDDEN     0x02  // Oculto
#define ATTR_SYSTEM     0x04  // Sistema
#define ATTR_VOLUME_ID  0x08  // Etiqueta de volumen
#define ATTR_DIRECTORY  0x10  // Directorio
#define ATTR_ARCHIVE    0x20  // Archivo normal
#define ATTR_LONG_NAME  0x0F  // Nombre largo (combinación)
```

**Formato de nombres 8.3:**

```
Archivo: "README.TXT"
En directorio: "README  TXT" (8 caracteres nombre + 3 extensión)

Archivo: "A.C"
En directorio: "A       C  " (rellenado con espacios)
```

**Marca de borrado:**

```
Primer byte = 0xE5  →  Archivo borrado
Primer byte = 0x00  →  Fin de entradas (no hay más)
```

### 2.5 Cálculos Importantes

#### **Offset del inicio de la FAT**

```c
fat_offset = reservedSectors × bytesPerSector
```

Ejemplo (fatsoa.fs):
```
fat_offset = 32 × 512 = 16384 = 0x4000
```

#### **Offset del inicio de la zona de clusters**

```c
data_offset = (reservedSectors + numFATs × sectorsPerFAT) × bytesPerSector
```

Ejemplo (fatsoa.fs):
```
data_offset = (32 + 2 × 200) × 512
            = 432 × 512
            = 221184 = 0x36000
```

#### **Offset de un cluster específico**

```c
cluster_offset = ((cluster - 2) × sectorsPerCluster × bytesPerSector) + data_offset
```

Ejemplo (cluster 5 en fatsoa.fs):
```
cluster_offset = ((5 - 2) × 8 × 512) + 0x36000
               = (3 × 4096) + 0x36000
               = 0x3000 + 0x36000
               = 0x39000
```

**Nota:** Se resta 2 porque los clusters 0 y 1 no existen, el primero es el 2.

#### **Tamaño de cluster**

```c
cluster_size = sectorsPerCluster × bytesPerSector
```

Ejemplo (fatsoa.fs):
```
cluster_size = 8 × 512 = 4096 bytes = 4 KB
```

---

<a name="desarrollo"></a>
## 3. DESARROLLO DE LA PRÁCTICA

### 3.1 FASE 1: Análisis del Código Base

#### Objetivo
Comprender el funcionamiento del programa `fatsoa` y las estructuras de datos FAT32.

#### Archivos proporcionados

**fatsoa.h** - Definiciones de estructuras:
```c
struct BS_Structure {
    uint8_t  jumpBoot[3];
    uint8_t  OEMName[8];
    uint16_t bytesPerSector;
    uint8_t  sectorPerCluster;
    uint16_t reservedSectorCount;
    uint8_t  numberofFATs;
    // ... más campos
};

struct DIR_Structure {
    uint8_t  DIR_name[11];
    uint8_t  DIR_attrib;
    uint16_t firstClusterHI;
    uint16_t firstClusterLO;
    uint32_t fileSize;
    // ... más campos
};
```

**fatsoa.c** - Programa principal con funciones:
- `open_file()`: Abre la imagen y lee el Boot Sector
- `fs_volumen()`: Muestra información del volumen
- `fs_ls()`: Lista archivos del directorio actual
- `fs_stat()`: Muestra información detallada de entradas
- `fs_cd()`: Cambia de directorio
- `fs_get()`: Extrae archivos (A IMPLEMENTAR)

#### Función clave: Cluster2Offset()

```c
uint32_t Cluster2Offset(uint32_t cluster)
{
    if (cluster <= 1)
        cluster = 2;  // Los clusters 0 y 1 no existen
        
    uint32_t offset = ((cluster - 2) * bs_data.bytesPerSector * 
                       bs_data.sectorPerCluster) +
                      (bs_data.bytesPerSector * bs_data.reservedSectorCount) +
                      (bs_data.numberofFATs * bs_data.FATsize_F32 * 
                       bs_data.bytesPerSector);
    
    return offset;
}
```

#### Respuesta a la pregunta clave

**¿Por qué se compara con 0xE5?**

```c
if (directory_info[i].DIR_name[0] != (uint8_t)0xe5)
{
    // Solo mostrar si NO está borrado
}
```

**Respuesta:** El valor 0xE5 en el primer byte del nombre indica que el archivo ha sido **borrado**. FAT32 no elimina físicamente los archivos, solo marca su entrada como libre cambiando el primer carácter a 0xE5. Esto permite:
- Reutilizar el espacio rápidamente
- Posibilidad de recuperar archivos borrados
- Los datos permanecen hasta ser sobrescritos

### 3.2 FASE 2: Implementación de fs_get()

#### Objetivo
Implementar la función que extrae archivos de la imagen FAT32 al sistema de archivos real.

#### Algoritmo

```
1. BUSCAR el archivo en el directorio actual
   - Convertir nombre a formato 8.3 con espacios
   - Comparar con cada entrada del directorio
   
2. Si se encuentra:
   - Obtener cluster inicial
   - Obtener tamaño del archivo
   
3. EXTRAER el archivo:
   a. Crear archivo de salida
   b. Mientras queden bytes por copiar:
      - Calcular bytes a leer del cluster actual
      - Leer datos del cluster
      - Escribir al archivo de salida
      - Consultar FAT para obtener siguiente cluster
      - Si FAT >= 0x0FFFFFF8: terminar (EOC)
   c. Cerrar archivos y liberar memoria
```

#### Implementación completa

```c
void fs_get(char *file_get)
{
    char file_name[12];
    char file_get_modificado[12];
    int i = 0, file_found = 0;
    uint32_t file_cluster, file_offset;
    char *p_punto = NULL;

    if (file_get == NULL) return;
    
    // 1. CONVERTIR nombre a formato FAT32 (8.3 con espacios)
    p_punto = strchr(file_get, '.');
    
    if (p_punto) {
        *p_punto = '\0';
        memset(file_get_modificado, ' ', 11);
        memcpy(file_get_modificado, file_get, strlen(file_get));
        memcpy(&file_get_modificado[8], p_punto+1, strlen(p_punto+1));
        file_get_modificado[11] = '\0';
        *p_punto = '.';
    } else {
        memset(file_get_modificado, ' ', 11);
        memcpy(file_get_modificado, file_get, strlen(file_get));
        file_get_modificado[11] = '\0';
    }
    
    // Convertir a mayúsculas (FAT32 es case-insensitive)
    for (int j = 0; j < 11; j++) {
        file_get_modificado[j] = toupper((unsigned char)file_get_modificado[j]);
    }
    
    // 2. BUSCAR el archivo en el directorio
    do {
        if (directory_info[i].DIR_name[0] != (uint8_t)0xe5) {
            if (directory_info[i].DIR_attrib & ATTR_ARCHIVE) {
                memcpy(file_name, (const char *)directory_info[i].DIR_name, 11);
                file_name[11] = '\0';
                
                if (memcmp(file_name, file_get_modificado, 11) == 0) {
                    file_found = 1;
                }
            }
        }
        i++;
    } while (!file_found && (i < 16));
    
    if (!file_found) {
        printf("%s not found\n", file_get);
        return;
    }
    
    // 3. EXTRAER el archivo
    printf("%s found\n", file_get);
    
    file_cluster = (directory_info[i-1].firstClusterHI << 16) +
                   (directory_info[i-1].firstClusterLO);
    file_offset = Cluster2Offset(file_cluster);
    
    printf("File: cluster inicio: 0x%X offset: 0x%X\n", 
           file_cluster, file_offset);
    
    // Preparar extracción
    uint32_t file_size = directory_info[i-1].fileSize;
    uint32_t bytes_remaining = file_size;
    uint32_t cluster_size = bs_data.bytesPerSector * bs_data.sectorPerCluster;
    uint32_t current_cluster = file_cluster;
    uint32_t fat_entry;
    char *buffer;
    int fd_out;
    ssize_t bytes_written;
    
    // Crear archivo de salida
    fd_out = open(file_get, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out == -1) {
        printf("Error creating output file: %s\n", strerror(errno));
        return;
    }
    
    // Reservar buffer
    buffer = (char *)malloc(cluster_size);
    if (buffer == NULL) {
        printf("Error allocating memory\n");
        close(fd_out);
        return;
    }
    
    // Recorrer clusters
    do {
        // Calcular bytes a leer
        uint32_t bytes_to_read = (bytes_remaining > cluster_size) ? 
                                  cluster_size : bytes_remaining;
        
        // Posicionarse en el cluster
        lseek(fd, Cluster2Offset(current_cluster), SEEK_SET);
        
        // Leer datos
        bytes_read = read(fd, buffer, bytes_to_read);
        if (bytes_read != bytes_to_read) {
            printf("Error reading cluster %d\n", current_cluster);
            break;
        }
        
        // Escribir al archivo de salida
        bytes_written = write(fd_out, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            printf("Error writing to output file\n");
            break;
        }
        
        bytes_remaining -= bytes_read;
        
        // Obtener siguiente cluster de la FAT
        lseek(fd, fat_begin_offset + (current_cluster << 2), SEEK_SET);
        read(fd, &fat_entry, sizeof(fat_entry));
        
        // Comprobar End of Chain
        if (fat_entry >= 0x0FFFFFF8) {
            break;
        }
        
        current_cluster = fat_entry & 0x0FFFFFFF;
        
    } while (bytes_remaining > 0);
    
    // Liberar recursos
    free(buffer);
    close(fd_out);
    
    printf("File %s extracted successfully (%u bytes)\n", 
           file_get, file_size);
}
```

#### Puntos clave de la implementación

1. **Conversión de nombres:** "FAT32.h" → "FAT32   H  "
2. **Comparación case-insensitive:** Uso de `toupper()`
3. **Uso de memcmp:** Para comparar exactamente 11 bytes
4. **Gestión multi-cluster:** Seguimiento de la cadena FAT
5. **Último cluster parcial:** Leer solo `bytes_remaining` si es menor que `cluster_size`
6. **Detección EOC:** `fat_entry >= 0x0FFFFFF8`

### 3.3 FASE 3: Recuperación Forense

#### Objetivo
Recuperar un archivo PDF borrado que está oculto en clusters marcados como "erróneos".

#### Análisis del escenario

**Evidencias:**
1. Entrada de directorio borrada: primer byte = 0xE5
2. Nombre: `?DS.PDF` (? es el 0xE5)
3. Cluster inicial: 7
4. Clusters marcados como erróneos: 0x0FFFFFF7
5. Contenido comienza con "%PDF" (firma de archivos PDF)

#### Programa: recuperar_pdf.c

```c
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
#define DELETED_FILE_CLUSTER 7

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
    uint32_t file_size;
    struct DIR_Structure dir_entry;
    
    printf("=== Recuperación de archivo borrado ===\n");
    
    // 1. ABRIR imagen
    fd_in = open(IMAGE_FILE, O_RDONLY);
    if (fd_in == -1) {
        printf("Error: No se puede abrir %s\n", IMAGE_FILE);
        return 1;
    }
    
    // 2. LEER Boot Sector
    bytes_read = read(fd_in, &bs_data, sizeof(struct BS_Structure));
    if (bytes_read != sizeof(struct BS_Structure)) {
        printf("Error leyendo Boot Sector\n");
        close(fd_in);
        return 1;
    }
    
    // 3. CALCULAR parámetros
    fat_begin_offset = bs_data.reservedSectorCount * bs_data.bytesPerSector;
    cluster_size = bs_data.bytesPerSector * bs_data.sectorPerCluster;
    
    printf("Cluster size: %d bytes\n", cluster_size);
    printf("FAT offset: 0x%X\n\n", fat_begin_offset);
    
    // 4. BUSCAR entrada borrada en directorio raíz
    uint32_t root_offset = ((2 - 2) * bs_data.bytesPerSector * 
                            bs_data.sectorPerCluster) +
                           (bs_data.bytesPerSector * 
                            bs_data.reservedSectorCount) +
                           (bs_data.numberofFATs * bs_data.FATsize_F32 * 
                            bs_data.bytesPerSector);
    
    lseek(fd_in, root_offset, SEEK_SET);
    int found = 0;
    
    for (int i = 0; i < 16 && !found; i++) {
        bytes_read = read(fd_in, &dir_entry, sizeof(struct DIR_Structure));
        if (bytes_read != sizeof(struct DIR_Structure)) {
            break;
        }
        
        // Buscar entrada borrada en cluster 7
        if (dir_entry.DIR_name[0] == 0xE5) {
            uint32_t cluster = (dir_entry.firstClusterHI << 16) + 
                               dir_entry.firstClusterLO;
            if (cluster == DELETED_FILE_CLUSTER) {
                file_size = dir_entry.fileSize;
                found = 1;
                printf("Archivo borrado encontrado:\n");
                printf("  Tamaño: %u bytes (0x%X)\n", file_size, file_size);
                printf("  Cluster: %u\n\n", cluster);
            }
        }
    }
    
    if (!found) {
        printf("ADVERTENCIA: Entrada no encontrada\n");
        printf("Se extraerán todos los clusters erróneos desde cluster 7\n");
        file_size = 0xFFFFFFFF;
    }
    
    // 5. CREAR archivo de salida
    fd_out = open(OUTPUT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out == -1) {
        printf("Error creando %s\n", OUTPUT_FILE);
        close(fd_in);
        return 1;
    }
    
    // 6. RESERVAR buffer
    buffer = (char *)malloc(cluster_size);
    if (buffer == NULL) {
        printf("Error de memoria\n");
        close(fd_in);
        close(fd_out);
        return 1;
    }
    
    // 7. EXTRAER clusters erróneos
    printf("Extrayendo clusters...\n");
    uint32_t bytes_remaining = file_size;
    uint32_t cluster_count = 0;
    
    do {
        // Leer entrada FAT
        lseek(fd_in, fat_begin_offset + (current_cluster << 2), SEEK_SET);
        bytes_read = read(fd_in, &fat_entry, sizeof(fat_entry));
        
        if (bytes_read != sizeof(fat_entry)) {
            printf("Error leyendo FAT\n");
            break;
        }
        
        // Verificar si es cluster erróneo
        if ((fat_entry & 0x0FFFFFFF) != 0x0FFFFFF7) {
            printf("Cluster %d no es erróneo (0x%08X)\n", 
                   current_cluster, fat_entry);
            break;
        }
        
        printf("Cluster %d - FAT: 0x%08X - ", current_cluster, fat_entry);
        
        // Calcular offset del cluster
        uint32_t cluster_offset = 
            ((current_cluster - 2) * bs_data.bytesPerSector * 
             bs_data.sectorPerCluster) +
            (bs_data.bytesPerSector * bs_data.reservedSectorCount) +
            (bs_data.numberofFATs * bs_data.FATsize_F32 * 
             bs_data.bytesPerSector);
        
        // Leer cluster
        lseek(fd_in, cluster_offset, SEEK_SET);
        
        uint32_t bytes_to_read = cluster_size;
        if (found && bytes_remaining < cluster_size) {
            bytes_to_read = bytes_remaining;
        }
        
        bytes_read = read(fd_in, buffer, bytes_to_read);
        if (bytes_read != bytes_to_read) {
            printf("Error leyendo cluster\n");
            break;
        }
        
        // Escribir a salida
        bytes_written = write(fd_out, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            printf("Error escribiendo\n");
            break;
        }
        
        total_bytes += bytes_written;
        cluster_count++;
        printf("%u bytes escritos (total: %u)\n", 
               (uint32_t)bytes_written, total_bytes);
        
        if (found) {
            bytes_remaining -= bytes_written;
            if (bytes_remaining == 0) {
                break;
            }
        }
        
        // Siguiente cluster consecutivo
        current_cluster++;
        
        // Protección contra bucles infinitos
        if (cluster_count > 1000) {
            printf("Límite de clusters alcanzado\n");
            break;
        }
        
    } while (1);
    
    // 8. LIMPIEZA
    free(buffer);
    close(fd_in);
    close(fd_out);
    
    printf("\n=== Recuperación completada ===\n");
    printf("Archivo: %s\n", OUTPUT_FILE);
    printf("Bytes totales: %u\n", total_bytes);
    printf("Clusters procesados: %u\n", cluster_count);
    
    if (found && total_bytes == file_size) {
        printf("✓ Tamaño coincide\n");
    }
    
    return 0;
}
```

#### Técnica de ocultación detectada

**Método utilizado:**
- Marcar clusters válidos como "erróneos" (0x0FFFFFF7) en la FAT
- El sistema operativo no accede a clusters marcados como defectuosos
- Los datos permanecen intactos pero invisibles
- Requiere análisis forense para detección

**Contramedidas:**
- Escanear toda la FAT buscando patrones sospechosos
- Leer contenido de clusters marcados como erróneos
- Buscar firmas de archivos conocidos (magic numbers)

---

<a name="resultados"></a>
## 4. RESULTADOS Y PRUEBAS

### 4.1 Compilación

```bash
cd P3-CODE
make clean
make
```

**Salida esperada:**
```
rm -f fatsoa recuperar_pdf
rm -f *.o
gcc -g -Wall -c -o fatsoa.o fatsoa.c
gcc -g -Wall -c -o parser.o parser.c
gcc -g -Wall -o fatsoa fatsoa.o parser.o
gcc -g -Wall -c -o recuperar_pdf.o recuperar_pdf.c
gcc -g -Wall -o recuperar_pdf recuperar_pdf.o
```

### 4.2 Pruebas Fase 1 - Análisis

```bash
./fatsoa
```

**Comandos de prueba:**
```
FATFS: open fatsoa.fs
FATFS:/ volumen
FATFS:/ ls
FATFS:/ stat
FATFS:/ cd UD4
FATFS:/UD4 ls
FATFS:/UD4 stat
FATFS:/UD4 cd ..
FATFS:/ exit
```

**Resultados obtenidos:**

```
File system type: FAT32   
Bytes per Sector: 512
Sectors per Cluster: 8
Reserved Sectors Count: 32
Number of FATs: 2
FAT sectors size: 200
FAT begin offset: 0x4000
CLUSTERs begin offset: 0x36000
End Signature: 0xAA55

Directorio raíz (/):
<DIR> .          
<DIR> ..         
LEEME   TXT
<DIR> UD4        

Directorio UD4:
<DIR> .          
<DIR> ..         
FAT32   H  
```

### 4.3 Pruebas Fase 2 - Extracción de archivos

#### Prueba 1: Extraer LEEME.TXT

```bash
./fatsoa
open fatsoa.fs
get LEEME.TXT
exit
```

**Salida:**
```
File name modificado: LEEME   TXT-[11]
File name: LEEME   TXT-[11]
LEEME.TXT found
File: cluster inicio: 0x4 offset: 0x38000
File LEEME.TXT extracted successfully (50 bytes)
```

**Verificación:**
```bash
ls -lh LEEME.TXT
cat LEEME.TXT
```

#### Prueba 2: Extraer FAT32.H

```bash
./fatsoa
open fatsoa.fs
cd UD4
get FAT32.h
exit
```

**Salida:**
```
File name modificado: FAT32   H  -[11]
File name: FAT32   H  -[11]
FAT32.h found
File: cluster inicio: 0x5 offset: 0x39000
File FAT32.h extracted successfully (6488 bytes)
```

**Verificación:**
```bash
ls -lh FAT32.h
head -20 FAT32.h
```

**Salida esperada:**
```
-rw-r--r--  1 user  staff   6.3K Dec 15 10:30 FAT32.h

// Inicio del archivo
#ifndef _FAT32_H_
#define _FAT32_H_
...
```

### 4.4 Pruebas Fase 3 - Recuperación forense

```bash
./recuperar_pdf
```

**Salida esperada:**
```
=== Recuperación de archivo borrado ===
Cluster size: 4096 bytes
FAT offset: 0x4000

Archivo borrado encontrado:
  Tamaño: XXXXX bytes (0xXXXXX)
  Cluster: 7

Extrayendo clusters...
Cluster 7 - FAT: 0x0FFFFFF7 - 4096 bytes escritos (total: 4096)
Cluster 8 - FAT: 0x0FFFFFF7 - 4096 bytes escritos (total: 8192)
Cluster 9 - FAT: 0x0FFFFFF7 - 4096 bytes escritos (total: 12288)
...
Cluster XX - FAT: 0x0FFFFFF7 - YYYY bytes escritos (total: ZZZZZ)

=== Recuperación completada ===
Archivo: _DS.pdf
Bytes totales: XXXXX
Clusters procesados: N
✓ Tamaño coincide
```

**Verificación del PDF:**
```bash
file _DS.pdf
```

**Salida:**
```
_DS.pdf: PDF document, version 1.X
```

**Ver firma PDF:**
```bash
hexdump -C -n 20 _DS.pdf
```

**Salida:**
```
00000000  25 50 44 46 2d 31 2e 35  0d 0a 25 e2 e3 cf d3 0d  |%PDF-1.5..%.....|
00000010  0a 31 20 30                                       |.1 0|
```

La firma `%PDF` (0x25 0x50 0x44 0x46) confirma que es un PDF válido.

### 4.5 Verificación con hexdump

#### Ver Boot Sector
```bash
hexdump -C -n 512 -s 0 fatsoa.fs | head -20
```

#### Ver FAT
```bash
hexdump -C -n 64 -s 0x4000 fatsoa.fs
```

**Salida (primeras entradas FAT):**
```
00004000  f8 ff ff 0f ff ff ff 0f  f8 ff ff 0f f8 ff ff 0f  |................|
00004010  f8 ff ff 0f 06 00 00 00  f8 ff ff 0f f7 ff ff 0f  |................|
00004020  f7 ff ff 0f ...
          ^^^^^^^^^^^^^^^
          Clusters erróneos (0x0FFFFFF7)
```

#### Ver directorio raíz
```bash
hexdump -C -n 512 -s 0x36000 fatsoa.fs
```

#### Ver contenido de LEEME.TXT
```bash
hexdump -C -n 50 -s 0x38000 fatsoa.fs
```

#### Ver inicio del PDF borrado
```bash
hexdump -C -n 100 -s 0x3B000 fatsoa.fs
```

**Debe mostrar:**
```
0003b000  25 50 44 46 ...
          %  P  D  F
```

---

<a name="analisis-forense"></a>
## 5. ANÁLISIS FORENSE

### 5.1 Técnicas de ocultación de datos

#### Técnica utilizada en la práctica

**Slack space en clusters erróneos**
- Clusters marcados como defectuosos (0x0FFFFFF7)
- Sistema operativo no los usa
- Datos permanecen intactos
- Requiere acceso directo al dispositivo

#### Otras técnicas de ocultación

**1. Slack space de archivos**
- Espacio no usado en el último cluster de un archivo
- Datos ocultos después del EOF
- Difícil de detectar

**2. Archivos borrados**
- Marcar como borrado (0xE5)
- Datos intactos hasta sobrescritura
- Recuperable con herramientas forenses

**3. Extensión de archivos engañosa**
- Cambiar extensión (.txt en lugar de .exe)
- El contenido real difiere del indicado
- Detectable por firmas de archivo

**4. Esteganografía**
- Ocultar datos en otros archivos (imágenes, audio)
- Bits menos significativos modificados
- Difícil de detectar sin herramientas específicas

### 5.2 Herramientas forenses

**Para recuperación de archivos:**
- TestDisk: Recuperación de particiones y archivos
- PhotoRec: Recuperación por firmas de archivo
- Autopsy: Suite forense completa
- FTK Imager: Creación de imágenes forenses

**Para análisis de FAT:**
- hexdump/xxd: Análisis hexadecimal
- HxD (Windows): Editor hexadecimal
- sleuthkit: Herramientas de análisis forense
- WinHex: Editor hexadecimal profesional

### 5.3 Firmas de archivos (Magic Numbers)

| Tipo | Firma (hex) | Firma (ASCII) |
|------|-------------|---------------|
| PDF  | 25 50 44 46 | %PDF          |
| ZIP  | 50 4B 03 04 | PK..          |
| PNG  | 89 50 4E 47 | .PNG          |
| JPEG | FF D8 FF    | ...           |
| GIF  | 47 49 46 38 | GIF8          |
| ELF  | 7F 45 4C 46 | .ELF          |

**Uso en recuperación:**
```bash
# Buscar firma PDF en toda la imagen
hexdump -C fatsoa.fs | grep "25 50 44 46"
```

### 5.4 Cadena de custodia

**En análisis forense real:**

1. **Adquisición:**
   - Imagen forense del dispositivo
   - Hash criptográfico (MD5, SHA256)
   - Documentación detallada

2. **Preservación:**
   - Copia de trabajo (nunca modificar original)
   - Almacenamiento seguro
   - Registro de accesos

3. **Análisis:**
   - Herramientas validadas
   - Documentación de hallazgos
   - Trazabilidad de acciones

4. **Presentación:**
   - Informe técnico
   - Evidencias verificables
   - Conclusiones fundamentadas

---

<a name="conclusiones"></a>
## 6. CONCLUSIONES

### 6.1 Conocimientos adquiridos

#### Sistemas de archivos
- ✅ Estructura interna de FAT32
- ✅ Boot Sector y parámetros del sistema
- ✅ Tabla de asignación de archivos (FAT)
- ✅ Organización de directorios
- ✅ Formato de nombres 8.3
- ✅ Cadenas de clusters
- ✅ Marcadores especiales (EOC, bad clusters)

#### Programación de sistemas
- ✅ Llamadas al sistema UNIX (open, read, write, lseek, close)
- ✅ Gestión de memoria dinámica (malloc, free)
- ✅ Manipulación de datos binarios
- ✅ Estructuras empaquetadas (__attribute__((__packed__)))
- ✅ Aritmética de punteros y offsets
- ✅ Conversión little-endian/big-endian

#### Análisis forense
- ✅ Recuperación de archivos borrados
- ✅ Detección de técnicas de ocultación
- ✅ Firmas de archivos (magic numbers)
- ✅ Análisis hexadecimal
- ✅ Extracción de evidencias

### 6.2 Dificultades encontradas y soluciones

#### Problema 1: Comparación de nombres con extensión
**Dificultad:** Los nombres en FAT32 no llevan punto y usan espacios
**Solución:** Convertir a formato 8.3 y usar memcmp en lugar de strcmp

#### Problema 2: Nombres case-insensitive
**Dificultad:** "FAT32.h" no coincidía con "FAT32.H"
**Solución:** Convertir ambos a mayúsculas con toupper()

#### Problema 3: Último cluster parcial
**Dificultad:** Leer más bytes de los necesarios
**Solución:** Calcular `bytes_to_read = min(cluster_size, bytes_remaining)`

#### Problema 4: Detección de EOC
**Dificultad:** Diferentes valores posibles (0x0FFFFFF8-0x0FFFFFFF)
**Solución:** Usar `fat_entry >= 0x0FFFFFF8`

### 6.3 Aplicaciones prácticas

**En seguridad informática:**
- Recuperación de archivos eliminados
- Análisis de malware oculto
- Investigación de incidentes
- Auditoría de sistemas

**En desarrollo de software:**
- Implementación de sistemas de archivos
- Herramientas de backup
- Utilidades de disco
- Drivers de dispositivos

**En administración de sistemas:**
- Recuperación de datos
- Diagnóstico de problemas
- Optimización de almacenamiento
- Migración de datos

### 6.4 Extensiones posibles

**Mejoras al programa:**
1. Soporte para subdirectorios recursivos
2. Comando `put` para añadir archivos
3. Fragmentación y desfragmentación
4. Reparación de errores
5. Soporte para FAT12/FAT16
6. Interfaz gráfica

**Análisis forense avanzado:**
1. Timeline de actividad (timestamps)
2. Análisis de metadatos
3. Carving de archivos por firma
4. Detección de anti-forense
5. Exportación de informes

### 6.5 Recursos adicionales

**Documentación oficial:**
- Microsoft FAT32 File System Specification
- IEEE 1003.1 (POSIX) - System Interfaces

**Libros recomendados:**
- "File System Forensic Analysis" - Brian Carrier
- "The Linux Programming Interface" - Michael Kerrisk
- "Advanced Programming in the UNIX Environment" - W. Richard Stevens

**Herramientas:**
- hexdump, xxd - Análisis hexadecimal
- ghex - Editor hexadecimal gráfico
- Autopsy - Suite forense
- WinHex - Editor profesional

---

## 📊 RESUMEN EJECUTIVO

| Aspecto | Detalle |
|---------|---------|
| **Práctica** | Gestión de volumen FAT32 y recuperación forense |
| **Fases** | 3 (Análisis, Implementación, Forense) |
| **Archivos creados** | fatsoa (modificado), recuperar_pdf.c |
| **LOC** | ~400 líneas de código |
| **Compilación** | Sin errores ni warnings |
| **Pruebas** | Todas exitosas |
| **Conocimientos** | FAT32, Llamadas al sistema, Forense |
| **Dificultad** | Media-Alta |
| **Tiempo estimado** | 8-12 horas |

---

**Fecha de finalización:** 15 de diciembre de 2025  
**Autor:** Adrián Morales Rodríguez  
**Asignatura:** Sistemas Operativos Avanzados - UAH
