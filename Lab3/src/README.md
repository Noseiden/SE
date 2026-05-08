# Control de Temperatura e Iluminación con ESP32

Este proyecto implementa un sistema de control desarrollado en lenguaje C para una ESP32.

El programa permite medir y controlar variables de temperatura e iluminación en tiempo real. La temperatura deseada puede ser ingresada desde consola mediante comunicación UART, y el sistema ajusta automáticamente su comportamiento según la diferencia entre la temperatura medida y la temperatura configurada.

También se implementa control de intensidad luminosa mediante PWM, variando la salida de acuerdo con el nivel de iluminación detectado.

El sistema muestra periódicamente en consola la temperatura deseada, la temperatura medida, el porcentaje de iluminación y la velocidad de operación configurada.