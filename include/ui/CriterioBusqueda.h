#pragma once

#include "ui/Buscador.h"
#include "utils/Utils.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// El CRITERIO de busqueda de una entidad y la REGLA con la que se corre.
//
// POR QUE VIVEN ACA Y NO EN EL MOTOR CRUD
//
// El criterio lo usan TODAS las pantallas que buscan a una persona: las del
// motor CRUD (la pantalla "Buscar" y su buscador) y las que la buscan para
// otra cosa (agendar un turno, armar un reporte). Si cada una escribiera el
// suyo, habria N reglas de busqueda que se pueden desincronizar: "Gomez"
// encontraria una persona en una pantalla y otra en otra.
//
// LA REGLA DEL BUSCADOR
//
// El buscador de personas se comporta SIEMPRE igual:
//
//   - con el texto VACIO se listan las entidades ACTIVAS (son las que se
//     pueden usar);
//   - con texto se corren los CRITERIOS y se unen los resultados SIN
//     repetir (la misma persona puede coincidir en "nombre o apellido" y en
//     "dni");
//   - las celdas de cada coincidencia son las de las COLUMNAS del buscador.
//
// La regla vive ACA y la usan el motor CRUD y las pantallas nuevas: es la
// misma, escrita una vez.
// ---------------------------------------------------------------------------
namespace ui {

// Una coincidencia de una busqueda: el id de la entidad y sus celdas.
//
// EL ID VIAJA CON LA FILA
//
// El id NO se deduce de la posicion de una columna: la columna del id puede
// cambiar de lugar y el buscador devolveria la entidad equivocada. El que
// busca ya tiene la entidad en la mano, asi que el id lo devuelve con la
// fila.
//
// ES EL MISMO TIPO QUE EL DEL BUSCADOR
//
// El `ui::Buscador` devuelve las coincidencias de la misma manera: se reusa
// su tipo en vez de escribir dos veces la misma definicion.
using Coincidencia = Buscador::Coincidencia;

// Un criterio de busqueda: como se llama y que devuelve.
//
// DEVUELVE LAS COINCIDENCIAS
//
// El resultado de una busqueda son VARIAS coincidencias, y cada coincidencia
// son las celdas de las columnas MAS el id de la entidad. La pantalla
// "Buscar" las mete en una `ui::Tabla` (con las celdas) y el buscador elige
// una (con el id).
struct CriterioBusqueda {
    std::string etiqueta;
    std::function<std::vector<Coincidencia>(const std::string&)> buscar;
};

// El indice de la columna con ese nombre, o -1 si no existe.
//
// LA COLUMNA SE BUSCA POR NOMBRE
//
// La fila y las cabeceras tienen las columnas en el MISMO orden, asi que una
// celda se puede leer por la POSICION. Pero lo que se busca es una COLUMNA
// con un nombre ("dni"), no una posicion: si el orden de las columnas
// cambia, el buscador tiene que seguir mostrando lo mismo.
inline int indiceDeColumna(const std::vector<std::string>& cabeceras,
                            const std::string& nombre) {
    for (std::size_t i = 0; i < cabeceras.size(); ++i) {
        if (cabeceras[i] == nombre) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Las celdas de una fila para las columnas pedidas.
//
// UNA COLUMNA QUE NO EXISTE QUEDA VACIA
//
// Nunca se lee una celda de otro lado: si el nombre no calza, la celda
// queda vacia (y no se corre el indice).
inline std::vector<std::string> celdasDeLasColumnas(const std::vector<std::string>& fila,
                                                    const std::vector<std::string>& cabeceras,
                                                    const std::vector<std::string>& columnas) {
    std::vector<std::string> celdas;
    celdas.reserve(columnas.size());
    for (const std::string& nombre : columnas) {
        const int columna = indiceDeColumna(cabeceras, nombre);
        if (columna >= 0 && static_cast<std::size_t>(columna) < fila.size()) {
            celdas.push_back(fila[static_cast<std::size_t>(columna)]);
        } else {
            celdas.push_back("");
        }
    }
    return celdas;
}

// Si el id ya esta entre las coincidencias.
inline bool yaEstaEnLasCoincidencias(const std::vector<Coincidencia>& coincidencias, int id) {
    for (const Coincidencia& coincidencia : coincidencias) {
        if (coincidencia.first == id) {
            return true;
        }
    }
    return false;
}

// Las coincidencias de un BUSCADOR DE PERSONAS.
//
// `criterios` son los criterios de la entidad (los mismos que usa la
// pantalla "Buscar"). `activos` devuelve las entidades ACTIVAS con su fila
// (es lo que se muestra con el texto vacio). `proyectar` convierte una fila
// en las celdas que muestra el buscador: la proyeccion la hace la pantalla,
// que es la que declara las columnas.
inline std::vector<Coincidencia> coincidenciasDelBuscadorDePersonas(
    const std::vector<CriterioBusqueda>& criterios,
    const std::function<std::vector<Coincidencia>()>& activos,
    const std::function<std::vector<std::string>(const std::vector<std::string>&)>& proyectar,
    const std::string& texto) {
    std::vector<Coincidencia> coincidencias;

    // CON EL TEXTO VACIO SE MUESTRAN LOS ACTIVOS
    //
    // Los criterios NO encuentran nada con un texto vacio: el repositorio
    // define que una busqueda sin texto no encuentra nada (`buscarPorNombre`
    // devuelve una lista vacia) y el dni (y el legajo) se comparan EXACTOS,
    // asi que un texto vacio no es el dni de nadie.
    //
    // Pero el usuario que abre el buscador quiere VER la lista: si no, el
    // buscador arranca vacio y no dice nada. Por eso, con el texto vacio, se
    // muestran las entidades ACTIVAS: son las que se pueden usar, y son las
    // que el usuario espera ver al abrir el buscador.
    //
    // (Las dadas de baja se siguen BUSCANDO por nombre, dni o legajo: los
    // criterios las encuentran, asi que la reactivacion se puede hacer
    // escribiendo el nombre.)
    if (!Utils::esNoVacio(texto)) {
        for (const Coincidencia& activo : activos()) {
            coincidencias.push_back({activo.first, proyectar(activo.second)});
        }
        return coincidencias;
    }

    // SE CORREN LOS CRITERIOS Y SE UNEN LOS RESULTADOS
    //
    // Sin repetir: la misma persona puede coincidir en "nombre o apellido" y
    // en "dni", y en la lista tiene que aparecer UNA vez. Gana el primero que
    // aparece (el orden de los criterios es el de la configuracion).
    for (const CriterioBusqueda& criterio : criterios) {
        if (!criterio.buscar) {
            continue;
        }
        for (const Coincidencia& coincidencia : criterio.buscar(texto)) {
            if (yaEstaEnLasCoincidencias(coincidencias, coincidencia.first)) {
                continue;
            }
            coincidencias.push_back({coincidencia.first, proyectar(coincidencia.second)});
        }
    }
    return coincidencias;
}

} // namespace ui
