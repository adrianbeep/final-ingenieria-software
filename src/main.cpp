#include "ui/Aplicacion.h"
#include "ui/Contexto.h"
#include "ui/Pantallas/MenuPrincipal.h"

#include "exceptions/ExcepcionesSistema.h"
#include "repositories/RepositorioAlumnos.h"
#include "repositories/RepositorioProfesores.h"
#include "repositories/RepositorioTurnos.h"
#include "services/ServicioTurnos.h"

#include <iostream>
#include <memory>

// Punto de entrada. NO tiene logica de negocio (requisito 9 de la consigna):
// solo arma la aplicacion y la corre.
//
// El `try/catch` global vive ACA y no en la capa `ui`: la capa de interfaz
// no decide como se le informa un error al usuario.
int main() {
    try {
        // LOS REPOSITORIOS
        //
        // Son los mismos objetos que implementan el CRUD y la baja logica:
        // por eso el contexto los recibe una sola vez (el repositorio
        // concreto cumple los dos papeles).
        RepositorioAlumnos alumnos("data/alumnos.dat");
        RepositorioProfesores profesores("data/profesores.dat");
        RepositorioTurnos turnos("data/turnos.dat");

        // Los archivos de datos se cargan al arrancar. Si no existen, el
        // repositorio los crea vacios: la aplicacion no puede romperse por
        // un archivo faltante.
        alumnos.cargar();
        profesores.cargar();
        turnos.cargar();

        // EL SERVICIO DE REGLAS DE NEGOCIO
        ServicioTurnos servicioTurnos(alumnos, profesores, turnos);

        // EL CONTEXTO: lo que todas las pantallas necesitan.
        ui::Contexto contexto(alumnos, profesores, turnos, servicioTurnos);

        ui::Aplicacion aplicacion("Sistema de Turnos", contexto);

        aplicacion.ir([](ui::Aplicacion& app) {
            return std::make_unique<ui::Pantallas::MenuPrincipal>(app);
        });

        // Sin terminal interactiva se imprime el menu en texto plano y se
        // sale con codigo 0 (decision D31). Con terminal, arranca el ciclo
        // de eventos de FTXUI.
        aplicacion.ejecutar([&aplicacion]() { return aplicacion.textoPlanoActual(); });
    } catch (const ExcepcionSistema& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
