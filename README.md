# Time Glitch Arcade (MS-DOS)

Juego arcade de **micro-juegos clásicos** para **MS-DOS real**, desarrollado en C y orientado a hardware clásico (386/486).  
El proyecto es un viaje por distintas épocas del videojuego (70s → 80s → 90s), con una última parada paródica en el “infierno moderno” de los juegos móviles.

✅ **Este juego se ha hecho para el II Concurso de Videojuegos en MS-DOS del MS-DOS Club.**

Esta gente no sabe pasar página y sigue anclada a sistemas horribles que hubiera preferido pasaran al olvido.
Son geniales. Recibid mi más sincero agradecimiento desde aquí.

---

## Características principales

- Ejecutable **MS-DOS real** (sin dependencias modernas en runtime)
- **VGA modo 13h** (320×200, 256 colores)
- Arquitectura de **micro-juegos de una sola pantalla**
- Motor propio
- Doble buffer manual
- Dificultad global (Easy / Normal / Hard)
- Código fuente completamente disponible
- Modo turbo para psicópatas
- Sonido perforante de PC Speaker

---

## Requisitos

### Para jugar
- MS-DOS 6.22 o compatible
- CPU objetivo: **486DX66** (funciona también en 386, pero a pedales...)
- VGA compatible
- Teclado (joystick opcional)
- Recomendado para pruebas en PC moderno: DOSBox / DOSBox-X

### Para compilar
- **Open Watcom C/C++** (probado con Open Watcom 1.9)
- Entorno DOS real o DOSBox configurado para compilación
- **Python 3.x** (solo si se quiere regenerar assets desde PNG)

---

##  Compilación

El repositorio incluye un script de compilación listo para usar:

- `build.bat`

Pasos generales:
1. Configurar las variables de entorno de Open Watcom
2. Ejecutar `build.bat`
3. Se genera el ejecutable final para MS-DOS

> El proyecto se compila como **un único ejecutable**, sin dependencias externas en tiempo de ejecución.

---

## Assets y pipeline gráfico

Los PNG de trabajo están aquí:

- `TOOLS/RAW/`

En esa carpeta también se incluye:
- `palette.dat` (paleta VGA del proyecto)
- un script en **Python** que convierte los PNG a ficheros `.dat` usados por el juego

Los `.dat` generados son los que usa el ejecutable final.

> Nota: se incluyen tanto los assets finales como el pipeline para regenerarlos, con fines prácticos y educativos.

---

## Uso de IA

Tal y como parece estar estipulado hoy en día, **sí**, en este proyecto se ha utilizado **IA**.

- Se ha empleado IA en la **confección de algunas partes del código** (principalmente apoyo puntual, refactor y documentación).
- También se ha utilizado IA para la **generación de algunos assets gráficos** en determinados micro-juegos.

Dichos assets **no se usan de forma directa**: han sido **ajustados, modificados, redibujados y adaptados** manualmente para cumplir con las limitaciones reales del proyecto (resolución, paleta VGA, tamaños absurdos como 16×16, etc.).

Cualquiera que haya intentado generar iconos de **16×16 píxeles con IA** sabrá que eso no se consigue “apretando un botón”.

En resumen:
- Sí, hay **arte generado con IA**
- Sí, hay **intervención humana real**
- No, la IA **no ha hecho el juego sola**
- El objetivo ha sido **optimizar tiempo**, no sustituir criterio ni diseño
- Si no puedes vivir con ello, simplemente ignora este juego

---

## Licencia

Este proyecto es software libre.

El código fuente se distribuye bajo la licencia MIT (ver LICENSE).

Los assets (gráficos/sonido y materiales del pipeline) se incluyen para que el proyecto sea reproducible y modificable dentro del contexto del propio juego y del concurso.

---

## Créditos

Desarrollo: MC
Testeo: Un completo equipo de testeo con edades comprendidas entre los 6 y los 46 años.

Concurso: II Concurso de Videojuegos en MS-DOS — MS-DOS Club
