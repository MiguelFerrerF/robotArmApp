# Robot Arm Controller with Computer Vision Integration

## 📋 Descripción General
Este proyecto es una aplicación de escritorio robusta desarrollada en C++ utilizando el framework **Qt** y la biblioteca **OpenCV**. Su propósito principal es el control y gestión de un brazo robótico de 6 grados de libertad (6-DOF), integrando un sistema de visión artificial avanzado para tareas de "Pick and Place" autónomas.

El sistema permite el control manual del robot, la calibración precisa de la cámara y del sistema mano-ojo (Hand-Eye), y la detección de objetos en tiempo real mediante algoritmos de segmentación y transformación de coordenadas 3D.

---

## 🚀 Características Principales

### 🤖 Control del Robot
* **Cinemática Directa e Inversa:** Implementación completa de matrices de Denavit-Hartenberg (DH) para calcular la posición del efector final y los ángulos de las articulaciones.
* **Control Manual:** Interfaz gráfica (`RobotControlDialog`) para mover cada servo individualmente o en conjunto.
* **Gestión de Offsets:** Capacidad para leer, calibrar y guardar los offsets de los motores en memoria no volátil (o archivos JSON).

### 👁️ Visión Artificial
* **Captura Multihilo:** Manejo de cámara en un hilo dedicado (`VideoCaptureHandler`) para garantizar una interfaz fluida.
* **Procesamiento de Imagen:** Detección de objetos mediante segmentación (Canny/Contornos), cálculo de centroides y orientación de piezas.
* **Corrección de Perspectiva:** Herramienta interactiva para definir regiones de interés (ROI) y aplicar transformaciones de perspectiva ("warp").

### 📐 Calibración Avanzada
* **Calibración Intrínseca de Cámara:** Corrección de distorsión de lente mediante patrones de tablero de ajedrez (*Chessboard*).
* **Localización 3D:** Algoritmo de intersección Rayo-Plano para transformar coordenadas de píxeles (2D) a coordenadas del robot (3D).
* **Calibración Hand-Eye:** Implementación del método de Tsai para determinar la matriz de transformación entre la cámara y la base del robot ($RT_{cb}$).

### 🔌 Conectividad
* **Comunicación Serial:** Gestor Singleton (`SerialPortHandler`) para la comunicación UART con el microcontrolador del robot.
* **Protocolo ASCII:** Comandos legibles como `ANGLE:SERVO1:90`, `PLACE:...`, `OFFSET:...`.
* **Monitor Serial:** Consola de depuración integrada para visualizar el tráfico de datos.

---

## 🛠️ Arquitectura del Software
El proyecto sigue una arquitectura modular orientada a objetos, separando la lógica de negocio, la interfaz de usuario y el manejo de hardware.

### Módulos Principales

* **Core (MainWindow):**
    * Orquestador central que conecta los subsistemas de Visión y Robot.
    * Gestiona el flujo de estados (Monitoreo vs. Procesamiento).

* **Módulo de Robot (library-robot):**
    * `RobotHandler`: Cerebro cinemático. Resuelve $AX=B$ para la cinemática inversa y mantiene el estado geométrico ($RT_{bt}$).
    * `RobotControlDialog`: Interfaz de usuario para teleoperación.
    * `RobotCalibrationDialog`: Asistente para la calibración Hand-Eye automatizada.

* **Módulo de Visión (library-video):**
    * `VideoCaptureHandler`: Singleton que abstrae el hardware de la cámara (OpenCV VideoCapture). Aplica corrección de lente (undistort) en tiempo real.
    * `VideoProcessingDialog`: Pipeline de visión. Realiza recorte de perspectiva, segmentación y extracción de características (posición y ángulo).
    * `VideoCalibrationDialog`: Gestiona la matemática de proyección 2D $\to$ 3D.

* **Módulo de Comunicación (library-serial):**
    * `SerialPortHandler`: Wrapper sobre QSerialPort que asegura el acceso exclusivo al puerto.
    * `SerialConnectionSetupDialog`: Detección automática de puertos y configuración de Baud Rate.

---

## ⚙️ Dependencias y Requisitos
Para compilar y ejecutar este proyecto necesitas:
* **C++ Compiler:** Compatible con C++17 (MSVC, GCC, Clang).
* **Qt Framework:** Versión 6.x (Recomendado) o 5.15. Módulos: Core, Gui, Widgets, SerialPort.
* **OpenCV:** Versión 4.x. (Módulos core, imgproc, calib3d, videoio, imgcodecs).
* **CMake / QMake:** Sistema de construcción.

---

## 📖 Guía de Uso Rápida

1.  **Conexión**
    * Vaya a `Menu -> Serial -> Connect`.
    * Seleccione el puerto COM del robot (e.g., Arduino/ESP32) y la velocidad (e.g., 115200).
    * Vaya a `Menu -> Video -> Connect` para iniciar la cámara.

2.  **Calibración (Solo primera vez)**
    * **Cámara:** Abra `Calibration -> Camera`. Capture imágenes de un tablero de ajedrez en diferentes ángulos y ejecute la calibración para obtener la matriz intrínseca.
    * **Plano de Trabajo:** El sistema detectará automáticamente el plano de trabajo usando marcadores visuales o configuración manual para habilitar la localización 3D.

3.  **Procesamiento y Pick & Place**
    * Abra **Processing Video**.
    * Defina las 4 esquinas de la zona de trabajo (ROI) haciendo clic en la imagen.
    * Active el interruptor **"Start Processing"**.
    * El sistema dibujará contornos sobre las piezas detectadas y calculará sus coordenadas (X, Y, Z) y ángulo.
    * En la ventana principal, verifique las coordenadas calculadas y presione **"Pick and Place"** para ejecutar la maniobra.

---

## 📂 Estructura de Archivos Clave

```text
├── mainwindow.cpp/h            # Lógica principal y UI dashboard
├── library-robot/
│   ├── RobotHandler.cpp/h      # Cinemática Directa e Inversa
│   ├── RobotControlDialog...   # Control manual de servos
│   └── RobotCalibration...     # Calibración Hand-Eye
├── library-video/
│   ├── VideoCaptureHandler...  # Singleton de captura (QThread)
│   ├── VideoManagerDialog...   # Ajustes de cámara (Brillo, Foco, etc.)
│   ├── VideoCalibrationDialog. # Lógica matemática Rayo-Plano
│   └── VideoProcessingDialog.  # Algoritmos de visión (Canny, Warp)
└── library-serial/
    ├── SerialPortHandler...    # Gestión de QSerialPort
    └── SerialMonitorDialog...  # Consola de depuración
```

## 📝 Protocolo Serial
El robot espera y envía comandos terminados en `\n` bajo el siguiente formato:

* **Enviar (PC -> Robot):**
    * `SETUP:SERVO<id>:<angle>`: Mueve un servo específico.
    * `PLACE:<q1>:<q2>:<q3>:<q4>:<q5>:<claw>`: Ejecuta secuencia de movimiento compleja.
    * `READ:OFFSETS`: Solicita configuración de memoria.

* **Recibir (Robot -> PC):**
    * `ANGLE:SERVO<id>:<value>`: Feedback de posición actual.
    * `ANGLE_WITH_OFFSET:SERVO<id>:<value>`: Posición lógica.

---

## 🤝 Autoría y Créditos
Desarrollado como una solución integral para robótica educativa o industrial ligera.
El código hace uso intensivo de **OpenCV** para las transformaciones geométricas y **Qt** para la gestión asíncrona de eventos.

Proyecto desarrollado por Miguel Ferrer, Paula Fernandez y Raul Esperza.