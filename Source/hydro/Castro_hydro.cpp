#include "Castro.H"
#include "Castro_F.H"
#include "Castro_hydro_F.H"

#ifdef RADIATION
#include "Radiation.H"
#endif

using namespace amrex;

void
Castro::cons_to_prim(const Real time)
{

    BL_PROFILE("Castro::cons_to_prim()");
    
#ifdef RADIATION
    AmrLevel::FillPatch(*this, Erborder, NUM_GROW, time, Rad_Type, 0, Radiation::nGroups);

    MultiFab lamborder(grids, dmap, Radiation::nGroups, NUM_GROW);
    if (radiation->pure_hydro) {
      lamborder.setVal(0.0, NUM_GROW);
    }
    else {
      radiation->compute_limiter(level, grids, Sborder, Erborder, lamborder);
    }
#endif

    MultiFab& S_new = get_new_data(State_Type);

#ifdef _OPENMP
#pragma omp parallel
#endif
    for (MFIter mfi(S_new, hydro_tile_size); mfi.isValid(); ++mfi) {

        const Box& qbx = mfi.growntilebox(NUM_GROW);

        Array4<Real> const q_arr = q.array(mfi);
        Array4<Real> const qaux_arr = qaux.array(mfi);
        Array4<Real> const Sborder_arr = Sborder.array(mfi);
#ifdef RADIATION
        Array4<Real> const Erborder_arr = Erborder.array(mfi);
        Array4<Real> const lamborder_arr = lamborder.array(mfi);
#endif

        // Convert the conservative state to the primitive variable state.
        // This fills both q and qaux.

        ctoprim(qbx,
                time,
                Sborder_arr,
#ifdef RADIATION
                Erborder_arr,
                lamborder_arr,
#endif
                q_arr,
                qaux_arr);

    }

}

// Convert a MultiFab with conservative state data u to a primitive MultiFab q.
void
Castro::cons_to_prim(MultiFab& u, MultiFab& q_in, MultiFab& qaux_in, Real time)
{

    BL_PROFILE("Castro::cons_to_prim()");

    BL_ASSERT(u.nComp() == NUM_STATE);
    BL_ASSERT(q_in.nComp() == NQ);
    BL_ASSERT(u.nGrow() >= q_in.nGrow());

    int ng = q_in.nGrow();

#ifdef RADIATION
    AmrLevel::FillPatch(*this, Erborder, NUM_GROW, time, Rad_Type, 0, Radiation::nGroups);

    MultiFab lamborder(grids, dmap, Radiation::nGroups, NUM_GROW);
    if (radiation->pure_hydro) {
      lamborder.setVal(0.0, NUM_GROW);
    }
    else {
      radiation->compute_limiter(level, grids, Sborder, Erborder, lamborder);
    }
#endif

#ifdef _OPENMP
#pragma omp parallel
#endif
    for (MFIter mfi(u, TilingIfNotGPU()); mfi.isValid(); ++mfi) {

        const Box& bx = mfi.growntilebox(ng);

        auto u_arr = u.array(mfi);
#ifdef RADIATION
        auto Erborder_arr = Erborder.array(mfi);
        auto lamborder_arr = lamborder.array(mfi);
#endif
        auto q_in_arr = q_in.array(mfi);
        auto qaux_in_arr = qaux_in.array(mfi);

        ctoprim(bx,
                time,
                u_arr,
#ifdef RADIATION
                Erborder_arr,
                lamborder_arr,
#endif
                q_in_arr,
                qaux_in_arr);

    }

}

#ifdef TRUE_SDC
void
Castro::cons_to_prim_fourth(const Real time)
{

    BL_PROFILE("Castro::cons_to_prim_fourth()");
    
    // convert the conservative state cell averages to primitive cell
    // averages with 4th order accuracy

    const int* domain_lo = geom.Domain().loVect();
    const int* domain_hi = geom.Domain().hiVect();

    MultiFab& S_new = get_new_data(State_Type);

    // we don't support radiation here
#ifdef RADIATION
    amrex::Abort("radiation not supported to fourth order");
#else
#ifdef _OPENMP
#pragma omp parallel
#endif
    for (MFIter mfi(S_new, hydro_tile_size); mfi.isValid(); ++mfi) {

      const Box& qbx = mfi.growntilebox(NUM_GROW);
      const Box& qbxm1 = mfi.growntilebox(NUM_GROW-1);

      // note: these conversions are using a growntilebox, so it
      // will include ghost cells

      // convert U_avg to U_cc -- this will use a Laplacian
      // operation and will result in U_cc defined only on
      // NUM_GROW-1 ghost cells at the end.
      FArrayBox U_cc;
      U_cc.resize(qbx, NUM_STATE);

      ca_make_cell_center(BL_TO_FORTRAN_BOX(qbxm1),
                          BL_TO_FORTRAN_FAB(Sborder[mfi]),
                          BL_TO_FORTRAN_FAB(U_cc),
                          AMREX_ARLIM_ANYD(domain_lo), AMREX_ARLIM_ANYD(domain_hi));

      // enforce the minimum density on the new cell-centered state
      ca_enforce_minimum_density
        (AMREX_ARLIM_ANYD(qbxm1.loVect()), AMREX_ARLIM_ANYD(qbxm1.hiVect()),
         BL_TO_FORTRAN_ANYD(U_cc),
         verbose);

      // and ensure that the internal energy is positive
      reset_internal_energy(qbxm1, U_cc.array());

      // convert U_avg to q_bar -- this will be done on all NUM_GROW
      // ghost cells.
      auto Sborder_arr = Sborder.array(mfi);
      auto q_bar_arr = q_bar.array(mfi);
      auto qaux_bar_arr = qaux_bar.array(mfi);

      ctoprim(qbx,
              time, 
              Sborder_arr,
              q_bar_arr,
              qaux_bar_arr);

      // this is what we should construct the flattening coefficient
      // from

      // convert U_cc to q_cc (we'll store this temporarily in q,
      // qaux).  This will remain valid only on the NUM_GROW-1 ghost
      // cells.
      auto U_cc_arr = U_cc.array();
      auto q_arr = q.array(mfi);
      auto qaux_arr = qaux.array(mfi);

      ctoprim(qbxm1,
              time,
              U_cc_arr,
              q_arr,
              qaux_arr);
    }


    // check for NaNs
#ifndef AMREX_USE_CUDA
    check_for_nan(q);
    check_for_nan(q_bar);
#endif

#ifdef DIFFUSION
    // we need the cell-center temperature for the diffusion stencil,
    // so save it here, by copying from q (which is cell-center at the
    // moment).
    MultiFab::Copy(T_cc, q, QTEMP, 0, 1, NUM_GROW-1);
#endif

#ifdef _OPENMP
#pragma omp parallel
#endif
    for (MFIter mfi(S_new, hydro_tile_size); mfi.isValid(); ++mfi) {

      const Box& qbxm1 = mfi.growntilebox(NUM_GROW-1);

      // now convert q, qaux into 4th order accurate averages
      // this will create q, qaux in NUM_GROW-1 ghost cells, but that's
      // we need here

      ca_make_fourth_average(BL_TO_FORTRAN_BOX(qbxm1),
                             BL_TO_FORTRAN_FAB(q[mfi]),
                             BL_TO_FORTRAN_FAB(q_bar[mfi]),
                             AMREX_ARLIM_ANYD(domain_lo), AMREX_ARLIM_ANYD(domain_hi));

      // not sure if we need to convert qaux this way, or if we can
      // just evaluate it (we may not need qaux at all actually)
      ca_make_fourth_average(BL_TO_FORTRAN_BOX(qbxm1),
                             BL_TO_FORTRAN_FAB(qaux[mfi]),
                             BL_TO_FORTRAN_FAB(qaux_bar[mfi]),
                             AMREX_ARLIM_ANYD(domain_lo), AMREX_ARLIM_ANYD(domain_hi));

    }

#endif // RADIATION
}
#endif

void
Castro::check_for_cfl_violation(const Real dt)
{

    BL_PROFILE("Castro::check_for_cfl_violation()");

    auto dx = geom.CellSizeArray();

    MultiFab& S_new = get_new_data(State_Type);

    int ltime_integration_method = time_integration_method;

    ReduceOps<ReduceOpMax> reduce_op;
    ReduceData<Real> reduce_data(reduce_op);
    using ReduceTuple = typename decltype(reduce_data)::Type;

#ifdef _OPENMP
#pragma omp parallel
#endif
    for (MFIter mfi(S_new, hydro_tile_size); mfi.isValid(); ++mfi) {

        const Box& bx = mfi.tilebox();

        auto qaux_arr = qaux.array(mfi);
        auto q_arr = q.array(mfi);

        reduce_op.eval(bx, reduce_data,
        [=] AMREX_GPU_HOST_DEVICE (int i, int j, int k) noexcept -> ReduceTuple
        {
            // Compute running max of Courant number over grids

            Real dtdx = dt / dx[0];

            Real dtdy = 0.0_rt;
            if (AMREX_SPACEDIM >= 2) {
                dtdy = dt / dx[1];
            }

            Real dtdz = 0.0_rt;
            if (AMREX_SPACEDIM == 3) {
                dtdz = dt / dx[2];
            }

            Real courx = (qaux_arr(i,j,k,QC) + std::abs(q_arr(i,j,k,QU))) * dtdx;
            Real coury = (qaux_arr(i,j,k,QC) + std::abs(q_arr(i,j,k,QV))) * dtdy;
            Real courz = (qaux_arr(i,j,k,QC) + std::abs(q_arr(i,j,k,QW))) * dtdz;

            if (ltime_integration_method == 0) {

                // CTU integration constraint

                return {amrex::max(courx, coury, courz)};

#ifndef AMREX_USE_CUDA
                if (verbose == 1) {

                    if (courx > 1.0_rt) {
                        std::cout << std::endl;
                        std::cout << "Warning:: CFL violation in check_for_cfl_violation" << std::endl;
                        std::cout << ">>> ... (u+c) * dt / dx > 1 " << courx << std::endl;
                        std::cout << ">>> ... at cell i = " << i << " j = " << j << " k = " << k << std::endl;
                        std::cout << ">>> ... u = " << q_arr(i,j,k,QU) << " c = " << qaux_arr(i,j,k,QC) << std::endl;
                        std::cout << ">>> ... density = " << q_arr(i,j,k,QRHO) << std::endl;
                    }

                    if (coury > 1.0_rt) {
                        std::cout << std::endl;
                        std::cout << "Warning:: CFL violation in check_for_cfl_violation" << std::endl;
                        std::cout << ">>> ... (v+c) * dt / dx > 1 " << coury << std::endl;
                        std::cout << ">>> ... at cell i = " << i << " j = " << j << " k = " << k << std::endl;
                        std::cout << ">>> ... v = " << q_arr(i,j,k,QV) << " c = " << qaux_arr(i,j,k,QC) << std::endl;
                        std::cout << ">>> ... density = " << q_arr(i,j,k,QRHO) << std::endl;
                    }

                    if (courz > 1.0_rt) {
                        std::cout << std::endl;
                        std::cout << "Warning:: CFL violation in check_for_cfl_violation" << std::endl;
                        std::cout << ">>> ... (w+c) * dt / dx > 1 " << courz << std::endl;
                        std::cout << ">>> ... at cell i = " << i << " j = " << j << " k = " << k << std::endl;
                        std::cout << ">>> ... w = " << q_arr(i,j,k,QW) << " c = " << qaux_arr(i,j,k,QC) << std::endl;
                        std::cout << ">>> ... density = " << q_arr(i,j,k,QRHO) << std::endl;
                    }

                }
#endif
            }
            else {

                // method-of-lines constraint
                Real courtmp = courx;
                if (AMREX_SPACEDIM >= 2) {
                    courtmp += coury;
                }
                if (AMREX_SPACEDIM == 3) {
                    courtmp += courz;
                }

#ifndef AMREX_USE_CUDA
                if (verbose == 1) {

                    // note: it might not be 1 for all RK integrators
                    if (courtmp > 1.0_rt) {
                        std::cout << std::endl;
                        std::cout << "Warning:: CFL violation in check_for_cfl_violation" << std::endl;
                        std::cout << ">>> ... at cell i = " << i << " j = " << j << " k = " << k << std::endl;
                        std::cout << ">>> ... u = " << q_arr(i,j,k,QU) << " v = " << q_arr(i,j,k,QV)
                                  << " w = " << q_arr(i,j,k,QW) << " c = " << qaux_arr(i,j,k,QC) << std::endl;
                        std::cout << ">>> ... density = " << q_arr(i,j,k,QRHO) << std::endl;
                    }

                }
#endif

                return {courtmp};

            }

        });

    }

    ReduceTuple hv = reduce_data.value();
    Real courno = amrex::get<0>(hv);

    ParallelDescriptor::ReduceRealMax(courno);

    if (courno > 1.0) {
        amrex::Print() << "WARNING -- EFFECTIVE CFL AT LEVEL " << level << " IS " << courno << std::endl << std::endl;

        cfl_violation = 1;
    }

}
