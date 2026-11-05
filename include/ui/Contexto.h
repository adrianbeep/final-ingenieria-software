#pragma once

#include "repositories/RepositorioAlumnos.h"
#include "repositories/RepositorioProfesores.h"
#include "repositories/RepositorioTurnos.h"
#include "services/ServicioTurnos.h"
#include "ui/Mensaje.h"

#include <string>

// ---------------------------------------------------------------------------
// El contexto de la aplicacion: lo que TODAS las pantallas necesitan.
//
// QUE ES Y QUE NO ES
//
// Es el punto unico donde se juntan los repositorios y el servicio de turnos.
// No es un "contenedor de inyeccion" ni un service locator: son CUATRO
// referencias, una por cada collaborator, y todas se pasan por el
// constructor. Con tres repositorios, un contenedor seria ruido.
//
// POR QUE GUARDA REFERENCIAS Y NO PUNTEROS
//
// La vida de los repositorios es la de la aplicacion: viven en el `main`
// hasta que el programa termina. Guardarlos por referencia hace imposible
// el error clasico del puntero colgante, y deja claro (en la firma) que el
// contexto NO es dueño de nada.
//
// LOS REPOSITORIOS CONCRETOS, NO LAS INTERFACES
//
// El contexto expone los repositorios CONCRETOS (`RepositorioAlumnos`,
// `RepositorioProfesores` y `RepositorioTurnos`) y no sus interfaces.
//
// La razon son las REGLAS DE BUSQUEDA: esas reglas viven en el repositorio,
// que es quien tiene el conjunto de las entidades. `buscarPorNombre` es una
// coincidencia PARCIAL e insensible a mayusculas sobre el nombre o el
// apellido; `existeMatricula` y `existeLegajo` son una coincidencia EXACTA
// sobre la matricula y el legajo.
//
// Si el contexto entregara el repositorio por su INTERFAZ, esas reglas no
// estarian disponibles y la pantalla tendria que REESCRIBIRLAS: dos reglas
// que se pueden desincronizar, y la aplicacion contradiciendose a si misma.
//
// La pantalla que no necesita una regla concreta (el motor CRUD) sigue
// trabajando contra las interfaces: eso es lo que permite que sus tests
// injecten fakes. Lo que cambia es DE DONDE salen los repositorios, no el
// contrato que usa el motor.
//
// LA BAJA LOGICA NO ES UN REPOSITORIO MAS
//
// `IRepositorioBajaLogica<T>` NO es un cuarto y un quinto repositorio: es una
// INTERFAZ que el repositorio concreto ya implementa. El motor CRUD la
// recibe por parametro, y el mismo objeto concreto cumple los dos papeles a
// la vez. Por eso el contexto tiene TRES referencias y no CINCO.
//
// EL MENSAJE DE LA ULTIMA OPERACION
//
// Es el UNICO estado que se comparte ENTRE pantallas: el motor CRUD avisa
// "OK  Alumno #7 registrado." y el menu principal lo muestra cuando el
// usuario vuelve. Vive en el contexto (y no en una variable global) porque
// el contexto es justamente el objeto que las pantallas tienen en comun.
//
// OJO: el mensaje NO es un error del dominio: es un dato de la capa `ui`,
// asi que vive en `ui::Mensaje` y no en la capa de dominio.
// ---------------------------------------------------------------------------
namespace ui {

class Contexto {
public:
    // Los repositorios CONCRETOS: la pantalla trabaja contra la clase que
    // sabe las reglas de busqueda (ver el comentario de arriba).
    //
    // El servicio de turnos va aparte porque no es un repositorio: es la
    // capa de reglas de negocio, y por eso recibe los repositorios por
    // interfaz.
    Contexto(RepositorioAlumnos& alumnos,
             RepositorioProfesores& profesores,
             RepositorioTurnos& turnos,
             ServicioTurnos& servicioTurnos);

    RepositorioAlumnos& alumnos() const;
    RepositorioProfesores& profesores() const;
    RepositorioTurnos& turnos() const;
    ServicioTurnos& servicioTurnos() const;

    // EL MENSAJE DE LA ULTIMA OPERACION
    //
    // Lo escribe la pantalla que termina una operacion y lo lee la pantalla
    // a la que se vuelve. Es un DATO, no una impresion: el motor dibuja la
    // pantalla entera y un `std::cout` en el medio se cuela entre dos
    // frames (decision D38).
    void mostrarMensaje(const Mensaje& mensaje);
    const Mensaje& mensaje() const;

private:
    RepositorioAlumnos& _alumnos;
    RepositorioProfesores& _profesores;
    RepositorioTurnos& _turnos;
    ServicioTurnos& _servicioTurnos;

    // El ultimo mensaje. Empieza vacio: todavia no paso ninguna operacion.
    Mensaje _mensaje;
};

} // namespace ui
