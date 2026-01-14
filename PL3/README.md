# Práctica 3: Gestión de un volumen FAT32
## Sistemas Operativos Avanzados - UAH

### Autor
Adrián Morales Rodríguez

### Descripción
Esta práctica implementa las funcionalidades de lectura e interpretación de un volumen FAT32, 
incluyendo la extracción de archivos y la recuperación de archivos borrados.

---

## FASE 1: Análisis del código

### Compilación
```bash
cd P3-CODE
make
```

### Ejecución básica
```bash
./fatsoa
FATFS:/ open fatsoa.fs
FATFS:/ volumen
FATFS:/ ls
FATFS:/ stat
```

### Respuesta a las preguntas de la Fase 1:

**¿Por qué se compara el primer carácter del nombre con 0xE5?**

El valor 0xE5 en el primer byte del nombre de una entrada de directorio indica que el archivo 
ha sido borrado. Cuando un archivo se elimina en FAT32, no se borra físicamente del disco, 
sino que se marca como borrado cambiando el primer carácter de su nombre a 0xE5. Esto permite:

1. Indicar que el espacio está libre para ser reutilizado
2. Mantener los datos del archivo en el disco hasta que se sobrescriban
3. Posibilitar la recuperación de archivos borrados

Por eso el código no muestra las entradas que empiezan con 0xE5, ya que corresponden a 
archivos eliminados.

---

## FASE 2: Implementación de fs_get()

### Funcionalidad implementada

La función `fs_get()` ha sido completada con las siguientes características:

1. **Búsqueda del archivo**: Localiza el archivo en el directorio actual
2. **Lectura multi-cluster**: Soporta archivos que ocupan múltiples clusters
3. **Seguimiento de la FAT**: Utiliza la tabla FAT para seguir la cadena de clusters
4. **Gestión de tamaño**: Escribe exactamente el número de bytes indicado en fileSize
5. **Detección de fin de cadena**: Reconoce los marcadores EOC (End of Chain) >= 0x0FFFFFF8

### Algoritmo implementado:

```c
1. Obtener el cluster inicial y tamaño del archivo
2. Crear el archivo de salida con el mismo nombre
3. Mientras queden bytes por copiar:
   a. Calcular bytes a leer del cluster actual
   b. Leer datos del cluster
   c. Escribir datos al archivo de salida
   d. Consultar la FAT para obtener el siguiente cluster
   e. Si FAT >= 0x0FFFFFF8: es el último cluster, terminar
4. Cerrar archivos y liberar memoria
```

### Prueba de funcionamiento:

```bash
./fatsoa
FATFS:/ open fatsoa.fs
FATFS:/ ls
FATFS:/ get LEEME.TXT
FATFS:/ cd UD4
FATFS:/UD4 get FAT32.h
```

---

## FASE 3: Recuperación de archivo borrado

### Análisis forense

Según el enunciado:
- Existe un archivo PDF borrado en el directorio raíz: "?DS.PDF"
- El primer carácter es 0xE5 (marcador de archivo borrado)
- Cluster de inicio: 7
- Los clusters 7 y siguientes están marcados como erróneos (0x0FFFFFF7) en la FAT
- El contenido comienza con la firma PDF: "%PDF" (0x25 0x50 0x44 0x46)

### Programa recuperar_pdf.c

Se ha creado un programa independiente que:

1. **Lee el Boot Sector** para obtener parámetros del sistema de archivos
2. **Busca la entrada borrada** en el directorio raíz para obtener el tamaño exacto
3. **Extrae clusters consecutivos** marcados como erróneos (0x0FFFFFF7)
4. **Genera el archivo _DS.pdf** con el contenido recuperado
5. **Respeta el tamaño del archivo** escribiendo solo los bytes necesarios del último cluster

### Compilación y ejecución:

```bash
cd P3-CODE
make
./recuperar_pdf
```

### Salida esperada:

```
=== Programa de recuperación de fichero borrado ===
Imagen: fatsoa.fs
Fichero a recuperar: _DS.pdf
Cluster inicial: 7

Bytes por sector: 512
Sectores por cluster: 8
Tamaño del cluster: 4096 bytes
Offset de la FAT: 0x4000

Entrada de fichero borrado encontrada:
  Tamaño: XXXXX bytes (0xXXXX)
  Cluster inicial: 7

Extrayendo clusters marcados como erróneos (0x0FFFFFF7)...
Cluster 7 - FAT: 0x0FFFFFF7 (erróneo) - 4096 bytes escritos (total: 4096)
Cluster 8 - FAT: 0x0FFFFFF7 (erróneo) - 4096 bytes escritos (total: 8192)
...

=== Recuperación completada ===
Fichero generado: _DS.pdf
Total de bytes escritos: XXXXX
Clusters procesados: X
```

### Verificación del archivo recuperado:

```bash
file _DS.pdf          # Debe mostrar: PDF document
hexdump -C -n 20 _DS.pdf  # Debe mostrar la firma %PDF al inicio
```

---

## Estructura de archivos

```
P3-CODE/
├── fatsoa.c          - Programa principal (con fs_get completado)
├── fatsoa.h          - Estructuras FAT32
├── parser.c          - Parser de comandos
├── parser.h          - Cabecera del parser
├── recuperar_pdf.c   - Programa de recuperación (Fase 3)
├── Makefile          - Compilación de ambos programas
└── fatsoa.fs         - Imagen del volumen FAT32
```

---

## Conceptos clave implementados

### Llamadas al sistema utilizadas:
- `open()`: Apertura de archivos (imagen y salida)
- `read()`: Lectura de sectores, clusters, FAT
- `write()`: Escritura de datos extraídos
- `lseek()`: Posicionamiento en offsets específicos
- `close()`: Cierre de descriptores de archivo

### Cálculos FAT32:
- **Offset de FAT**: `reservedSectorCount × bytesPerSector`
- **Offset de cluster**: `((cluster - 2) × sectorPerCluster × bytesPerSector) + offsetDatosBase`
- **Entrada FAT**: `fatOffset + (cluster × 4)`
- **Tamaño de cluster**: `sectorPerCluster × bytesPerSector`

### Marcadores FAT32:
- **0x0FFFFFF8 - 0x0FFFFFFF**: End of Chain (último cluster)
- **0x0FFFFFF7**: Cluster erróneo (bad cluster)
- **0x00000000**: Cluster libre
- **0xE5**: Primer byte de entrada de directorio borrada

---

## Notas adicionales

- El código gestiona correctamente archivos que no ocupan completamente el último cluster
- Se implementa gestión de errores en todas las operaciones de E/S
- Los buffers se reservan dinámicamente con malloc()
- Se liberan todos los recursos correctamente (memoria, descriptores)
- El código es robusto ante imágenes corruptas o malformadas

---

## Autoevaluación completada

Para responder a las preguntas de la autoevaluación, ejecutar:

```bash
./fatsoa
FATFS:/ open fatsoa.fs
FATFS:/ volumen    # Ver sectores por cluster y sectores reservados
FATFS:/ stat       # Ver tamaños y clusters de inicio de archivos
```

Los offsets se pueden verificar con:
```bash
hexdump -C -n 512 -s <offset> fatsoa.fs
```
