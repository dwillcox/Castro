#include "Castro.H"
#include "Castro_F.H"

using namespace amrex;

void
Castro::ca_sdc_update_advection_o2_lobatto(const amrex::Box& bx,
                                           amrex::Real dt_m, amrex::Real dt,
                                           amrex::Array4<const amrex::Real> const& k_m,
                                           amrex::Array4<amrex::Real> const& k_n,
                                           amrex::Array4<const amrex::Real> const& A_m,
                                           amrex::Array4<const amrex::Real> const& A_0_old,
                                           amrex::Array4<const amrex::Real> const& A_1_old,
                                           int m_start)
{
    // update k_m to k_n via advection -- this is a second-order accurate update
    // for the Gauss-Lobatto discretization of the time nodes

    // here, dt_m is the update for this stage, from one time node to the next
    // dt is the update over the whole timestep, n to n+1

    // Gauss-Lobatto / trapezoid

    AMREX_PARALLEL_FOR_4D(bx, k_n.nComp(), i, j, k, n,
    {
        k_n(i,j,k,n) = k_m(i,j,k,n) + 0.5_rt * dt * (A_0_old(i,j,k,n) + A_1_old(i,j,k,n));
    });
}


void
Castro::ca_sdc_update_advection_o2_radau(const amrex::Box& bx,
                                         amrex::Real dt_m, amrex::Real dt,
                                         amrex::Array4<const amrex::Real> const& k_m,
                                         amrex::Array4<amrex::Real> const& k_n,
                                         amrex::Array4<const amrex::Real> const& A_m,
                                         amrex::Array4<const amrex::Real> const& A_0_old,
                                         amrex::Array4<const amrex::Real> const& A_1_old,
                                         amrex::Array4<const amrex::Real> const& A_2_old,
                                         int m_start)
{
    // update k_m to k_n via advection -- this is a second-order accurate update
    // for the Radau discretization of the time nodes

    // here, dt_m is the update for this stage, from one time node to the next
    // dt is the update over the whole timestep, n to n+1

    // Radau

    if (m_start == 0)
    {
        AMREX_PARALLEL_FOR_4D(bx, k_n.nComp(), i, j, k, n,
        {
            k_n(i,j,k,n) = k_m(i,j,k,n) +
                dt_m * (A_m(i,j,k,n) - A_0_old(i,j,k,n)) +
                dt/12.0_rt * (5.0_rt*A_1_old(i,j,k,n) - A_2_old(i,j,k,n));
        });
    }
    else if (m_start == 1)
    {
        AMREX_PARALLEL_FOR_4D(bx, k_n.nComp(), i, j, k, n,
        {
            k_n(i,j,k,n) = k_m(i,j,k,n) +
                dt_m * (A_m(i,j,k,n) - A_1_old(i,j,k,n)) +
                dt/3.0_rt * (A_1_old(i,j,k,n) + A_2_old(i,j,k,n));
        });
    }
}

void
Castro::ca_sdc_update_advection_o4_lobatto(const amrex::Box& bx,
                                         amrex::Real dt_m, amrex::Real dt,
                                         amrex::Array4<const amrex::Real> const& k_m,
                                         amrex::Array4<amrex::Real> const& k_n,
                                         amrex::Array4<const amrex::Real> const& A_m,
                                         amrex::Array4<const amrex::Real> const& A_0_old,
                                         amrex::Array4<const amrex::Real> const& A_1_old,
                                         amrex::Array4<const amrex::Real> const& A_2_old,
                                         int m_start)
{
    // update k_m to k_n via advection -- this is a fourth order accurate update

    // here, dt_m is the update for this stage, from one time node to the next
    // dt is the update over the whole timestep, n to n+1

    // Gauss-Lobatto (Simpsons)

    if (m_start == 0)
    {
        AMREX_PARALLEL_FOR_4D(bx, k_n.nComp(), i, j, k, n,
        {
            k_n(i,j,k,n) = k_m(i,j,k,n) +
                dt_m * (A_m(i,j,k,n) - A_0_old(i,j,k,n)) +
                dt/24.0_rt * (5.0_rt*A_0_old(i,j,k,n) + 8.0_rt*A_1_old(i,j,k,n) - A_2_old(i,j,k,n));
        });
    }
    else if (m_start == 1)
    {
        AMREX_PARALLEL_FOR_4D(bx, k_n.nComp(), i, j, k, n,
        {
            k_n(i,j,k,n) = k_m(i,j,k,n) +
                dt_m * (A_m(i,j,k,n) - A_1_old(i,j,k,n)) +
                dt/24.0_rt * (-A_0_old(i,j,k,n) + 8.0_rt*A_1_old(i,j,k,n) + 5.0_rt*A_2_old(i,j,k,n));
        });
    }
    else
        amrex::Abort("error in ca_sdc_update_advection_o4_lobatto -- should not be here");
}

void
Castro::ca_sdc_update_advection_o4_radau(const amrex::Box& bx,
                                         amrex::Real dt_m, amrex::Real dt,
                                         amrex::Array4<const amrex::Real> const& k_m,
                                         amrex::Array4<amrex::Real> const& k_n,
                                         amrex::Array4<const amrex::Real> const& A_m,
                                         amrex::Array4<const amrex::Real> const& A_0_old,
                                         amrex::Array4<const amrex::Real> const& A_1_old,
                                         amrex::Array4<const amrex::Real> const& A_2_old,
                                         amrex::Array4<const amrex::Real> const& A_3_old,
                                         int m_start)
{
    // update k_m to k_n via advection -- this is a fourth-order accurate update

    // here, dt_m is the update for this stage, from one time node to the next
    // dt is the update over the whole timestep, n to n+1

    // Gauss-Lobatto (Simpsons)

    if (m_start == 0)
    {
        AMREX_PARALLEL_FOR_4D(bx, k_n.nComp(), i, j, k, n,
        {
            k_n(i,j,k,n) = k_m(i,j,k,n) +
                dt_m * (A_m(i,j,k,n) - A_0_old(i,j,k,n)) +
                dt/1800.0_rt * ((-35.0_rt*sqrt(6.0_rt) + 440.0_rt)*A_1_old(i,j,k,n) +
                                (-169.0_rt*sqrt(6.0_rt) + 296.0_rt)*A_2_old(i,j,k,n) +
                                (-16.0_rt + 24.0_rt*sqrt(6.0_rt))*A_3_old(i,j,k,n));
        });
    }
    else if (m_start == 1)
    {
        AMREX_PARALLEL_FOR_4D(bx, k_n.nComp(), i, j, k, n,
        {
            k_n(i,j,k,n) = k_m(i,j,k,n) +
                dt_m * (A_m(i,j,k,n) - A_1_old(i,j,k,n)) +
                dt/150.0_rt * ((-12.0_rt + 17.0_rt*sqrt(6.0_rt))*A_1_old(i,j,k,n) +
                               (12.0_rt + 17.0_rt*sqrt(6.0_rt))*A_2_old(i,j,k,n) +
                               (-4.0_rt*sqrt(6.0_rt))*A_3_old(i,j,k,n));
        });
    }
    else if (m_start == 2)
    {
        AMREX_PARALLEL_FOR_4D(bx, k_n.nComp(), i, j, k, n,
        {
            k_n(i,j,k,n) = k_m(i,j,k,n) +
                dt_m * (A_m(i,j,k,n) - A_2_old(i,j,k,n)) +
                dt/600.0_rt * ((168.0_rt - 73.0_rt*sqrt(6.0_rt))*A_1_old(i,j,k,n) +
                               (120.0_rt + 5.0_rt*sqrt(6.0_rt))*A_2_old(i,j,k,n) +
                               (72.0_rt + 8.0_rt*sqrt(6.0_rt))*A_3_old(i,j,k,n));
        });
    }
    else
        amrex::Abort("error in ca_sdc_update_advection_o4_radau -- should not be here");
}




void Castro::ca_sdc_compute_initial_guess(const amrex::Box& bx,
                                          amrex::Array4<const amrex::Real> const& U_old,
                                          amrex::Array4<const amrex::Real> const& U_new,
                                          amrex::Array4<const amrex::Real> const& A_old,
                                          amrex::Array4<const amrex::Real> const& R_old,
                                          amrex::Array4<amrex::Real> const& U_guess,
                                          amrex::Real const dt_m, int const sdc_iteration)
{
    // compute the initial guess for the Newton solve
    // Here dt_m is the timestep to update from time node m to m+1

    if (sdc_iteration == 0)
    {
        AMREX_PARALLEL_FOR_4D(bx, U_guess.nComp(), i, j, k, n,
        {
            U_guess(i,j,k,n) = U_old(i,j,k,n) + dt_m * A_old(i,j,k,n) + dt_m * R_old(i,j,k,n);
        });
    }
    else
    {
        AMREX_PARALLEL_FOR_4D(bx, U_guess.nComp(), i, j, k, n,
        {
            U_guess(i,j,k,n) = U_new(i,j,k,n);
        });
    }

}


#ifdef REACTIONS
void Castro::ca_store_reaction_state(const amrex::Box& bx,
                                     amrex::Array4<const amrex::Real> const& R_old,
                                     amrex::Array4<const amrex::Real> const& state,
                                     amrex::Array4<amrex::Real> const& R_store)
{
// copy the data from the last node's reactive source to the state data

  // for R_store we use the indices defined in Castro_setup.cpp for
  // Reactions_Type
  int nspec = R_store.nComp()-2;

  AMREX_PARALLEL_FOR_4D(bx, nspec, i, j, k, n,
  {
    R_store(i,j,k,n) = R_old(i,j,k,UFS+n)/state(i,j,k,URHO);
  });

  AMREX_PARALLEL_FOR_3D(bx, i, j, k,
  {
    R_store(i,j,k,nspec+1-1) = R_old(i,j,k,UEDEN)/state(i,j,k,URHO);
    R_store(i,j,k,nspec+2-1) = R_old(i,j,k,UEDEN);
  });
}

#endif
