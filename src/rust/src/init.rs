use std::sync::Once;

use tracing::info;

use crate::{ORCHARD_PK, ORCHARD_VK};

#[cxx::bridge]
mod ffi {
    #[namespace = "init"]
    extern "Rust" {
        fn rayon_threadpool();
        fn zksnark_params(sprout_path: String, load_proving_keys: bool);
    }
}

static PROOF_PARAMETERS_LOADED: Once = Once::new();

fn rayon_threadpool() {
    rayon::ThreadPoolBuilder::new()
        .thread_name(|i| format!("zc-rayon-{}", i))
        .build_global()
        .expect("Only initialized once");
}

/// Loads the zk-SNARK parameters into memory (Orchard-only chain).
/// Only called once.
///
/// If `load_proving_keys` is `false`, the proving keys will not be loaded, making it
/// impossible to create proofs. This flag is for the Boost test suite.
///
/// The sprout_path parameter is kept for API compatibility but is unused on Orchard-only chain.
fn zksnark_params(_sprout_path: String, load_proving_keys: bool) {
    PROOF_PARAMETERS_LOADED.call_once(|| {
        // Juno Cash: Orchard-only chain - only load Orchard parameters
        // Sprout and Sapling parameters are not loaded as they are banned at consensus level

        // Generate Orchard parameters.
        info!(target: "main", "Loading Orchard parameters");
        let orchard_pk = load_proving_keys.then(orchard::circuit::ProvingKey::build);
        let orchard_vk = orchard::circuit::VerifyingKey::build();

        // Caller is responsible for calling this function once, so
        // these global mutations are safe.
        unsafe {
            ORCHARD_PK = orchard_pk;
            ORCHARD_VK = Some(orchard_vk);
        }
    });
}
