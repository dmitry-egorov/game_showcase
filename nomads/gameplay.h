#pragma once

#define GP_MAX_ENTITIES  1024
#define GP_MAX_BLUEPRINTS 128
#define GP_MAX_BUILDINGS  128
#define GP_MAX_JOBS_PER_BUILDING 8

lu_scexpr gp_world_size        = _size_f32(GM_WORLD_SIZE_X, GM_WORLD_SIZE_Y);
lu_scexpr gp_path_grid_size    = _size_i32(128, 128);
lu_scexpr gp_path_grid_area    = gp_path_grid_size.w * (u16)gp_path_grid_size.h;
lu_scexpr gp_path_grid_max_pos = i32x2 { .x = gp_path_grid_size.w - 1, .y = gp_path_grid_size.h - 1 };
lu_scexpr gp_path_grid_rect    = rect_i32x4 {.br = gp_path_grid_max_pos };

#include "graphics.h"
#include "gameplay_resources.h"

lu_scexpr gp_grid_cell_size_in_world = size_f32x2 {.w = gp_world_size.w / gx_grid_size.w, .h = gp_world_size.h / gx_grid_size.h };

struct avoidance_spec {
    f32 range;
    f32 weight;
};

struct gp_entity_type {
    const char* id;
    avoidance_spec avoidance;
    f32 build_distance;
};

struct gp_entity_t;
struct gp_building_t;
struct gp_person_t;

struct gp_job_type_t {
    const char* id;
};

struct gp_job_t {
    gp_person_t* worker;

    lu_chain_link_t vacant_jobs_link;

    lu_mt_i_nd type() -> gp_job_type_t&;
    lu_mt_i_nd workplace() -> gp_building_t&;
};

typedef lu_grid<u8x2, gp_path_grid_size> gp_path_grid;

struct gp_action_t {
    const char* id;
    f32 cooldown;
};

struct gp_person_t {
    struct {
        struct {
            gp_action_t* action;

            f32 eta;
        } perform;

        struct {
            gp_entity_t* from;
            gp_entity_t* to;
            gp_resource_type* res_type;

            u8 step;
        } deliver;

        struct {
            gp_entity_t* target;

            gp_resource_type* res_type;
            u32 build_points;

            u8 in_progress;
        } build;

        struct {
            gp_entity_t* from;
            gp_resource_type* res_type;

            u8 step;
        } gather;
    } task;

    gp_job_t* job;
    bool checked_in_at_workplace;

    gp_entity_t* approach_target;

    rotor_f32x2 target_orientation;

    lu_chain_link_t     person_link;
    lu_chain_link_t pathfinder_link;
    f32 pf_cost_to_goal; // cost to goal accumulated during path finding

    lu_mt approach_tick(gp_entity_t*) -> bool; // -> within range, finished

    lu_mt perform(gp_action_t*) -> bool; // -> finished

    lu_mt deliver_start(gp_entity_t*   from_p, gp_entity_t* to_p, gp_resource_type*) -> void;
    lu_mt   build_start(gp_entity_t* target_p) -> void;
    lu_mt  gather_start(gp_entity_t* from_p, gp_resource_type*) -> void;

    lu_mt_nd entity() -> gp_entity_t&;
};

enum gp_building_placement {
    gp_bp_land,
    gp_bp_shore,
};

struct gp_building_t {
    lu_chain_link_t building_link;

    // construction
    u8 build_points;
    gp_resource_set resources_remaining_for_build;
    i32 build_points_error; // value for the spending amount algorithm
    lu_chain_link_t under_construction_link;

    // workplace
    lu_listarr<gp_job_t, GP_MAX_JOBS_PER_BUILDING> jobs;
    lu_chain_link_t workplace_link;

    lu_mt_nd is_under_construction() const -> bool { return build_points > 0; }

    lu_mt_nd entity() -> gp_entity_t&;
};

struct gp_entity_prefab_t {
    const char* id;
    f32 size;

    f32x3 scale;
    gx_visual_t*          visual;
    gx_visual_t*     pile_visual; // ready to be gathered visual
    gx_visual_t* depleted_visual;

    u32 max_hit_points;
    gp_resource_set max_inner_resources;

    bool can_be_gathered;
    bool provides_resources;

    gp_entity_type* type;

    // building
    const char* ui_text;
    gx_visual_t* construction_visual;
    gp_resource_set resources_for_construction;
    u8 max_build_points; // the number of build actions required to build this building

    bool is_town_center;
    lu_listarr<gp_job_type_t*, GP_MAX_JOBS_PER_BUILDING> job_types;

    f32x2 entrance_offset;
    gp_building_placement placement;
};

struct gp_entity_t {
    const char* id;
    gp_entity_prefab_t* prefab_;

    f32x2       position;
    rotor_f32x2 orientation;

    u32 hit_points;

    // pathfinding
    gp_path_grid* path_grid;
    lu_chain_link_t pathfinding_target_link;
    lu_chain_(gp_person_t, pathfinder_link) pathfinders_chain;

    // resources
    gp_storage structural_resources;
    gp_storage storage;
    lu_chain_link_t resource_provider_link;

    union {
        gp_building_t building;
        gp_person_t     person;
    };

    lu_chain_link_t scenery_link;

    bool is_dynamic; // the entity is created dynamically during gameplay

    lu_mt_i_nd prefab() const -> gp_entity_prefab_t& { return *prefab_; }
    lu_mt_nd   get_approach_pos() const -> f32x2;
};

enum gp_ui_screen {
    GPSC_NONE,
    GPSC_ROOT,
    GPSC_QUIT,
    GPSC_BUILDINGS_LIST,
    GPSC_BUILD,
};
static struct gameplay_t {
    lu_profiler tick_profiler;
    lu_profiler draw_profiler;
    lu_profiler update_blockage_profiler;
    lu_profiler pathfinding_profiler;
    lu_profiler tick_people_profiler;

    // data
    lu_listarr<gp_entity_t, GP_MAX_ENTITIES> entities;

    lu_chain_(gp_entity_t  ,  scenery_link) sceneries;
    lu_chain_(gp_person_t  ,   person_link) people;
    lu_chain_(gp_building_t, building_link) buildings;

    lu_listarr<gp_path_grid, GP_MAX_BUILDINGS> path_grids;

    bool terrain_initialized;
    lu_grid_bit_4x4<gx_grid_size> grass_grid;
    lu_grid_bit_4x4<gp_path_grid_size> terrain_path_block_grid;
    lu_grid_bit_4x4<gp_path_grid_size> path_block_grid;

    gp_building_t* town_center;
    lu_chain_(gp_building_t, under_construction_link) buildings_under_construction;
    lu_chain_(gp_entity_t,  resource_provider_link) resource_providers;
    lu_chain_(gp_job_t, vacant_jobs_link) vacant_jobs;

    struct {
        gp_entity_type scenery;
        gp_entity_type building;
        gp_entity_type person;
    } entity_types;

    struct {
        gp_action_t take;
        gp_action_t store;
        gp_action_t construct;
        gp_action_t cut_down;
        gp_action_t chop;
    } actions;

    struct {
        gp_job_type_t delivery;
        gp_job_type_t construction;
        gp_job_type_t woodcutting;
    } job_types;

    struct {
        gp_resource_type log;
        gp_resource_type stone;
    } res_types;

    // settings
    lgx_camera default_camera;

    f32 construction_approach_distance;
    f32      scenery_approach_distance;

    f32 people_remove_grass_cooldown;
    f32 grow_grass_cooldown;

    f32 speed_sand;
    f32 speed_grass;

    f32 people_remove_grass_eta;
    f32 grow_grass_eta;

    // pathfinding
    lu_chain_(gp_entity_t, pathfinding_target_link) pathfinding_targets_chain;
    i32   pf_max_steps;
    i32x2 pf_last_node;

    // current state
    bool paused;

    f32 light_xy_turns;
    f32 light_yz_turns;
    color_f32x4   light_color;
    color_f32x4 ambient_color;
    gx_leaves_noise_spec leaves_noise_spec;

    // ui
    gp_ui_screen screen_id;
    gp_entity_prefab_t* selected_building_prefab;
    bool build_mouse_held;
    bool build_mouse_moved;
    f32x2 build_position;
    rotor_f32x2 build_orientation;
} gp = {};

lu_fn gp_make_entity(const gp_entity_t&) -> gp_entity_t&;
lu_fn gp_build(const char* name, gp_entity_prefab_t*, f32x2 position, rotor_f32x2 orientation) -> gp_building_t&;
lu_fn gp_finish_construction(gp_building_t*) -> void;

#define gd_rot(xy, yz, zx) _rotor_f32x4_euler(xy, yz, zx)

lu_fn_i_nd gp_ent_id(gp_entity_t* p) -> u16;

lu_fn_i_nd gp_entity_ptr(gp_building_t*) -> gp_entity_t*;
lu_fn_i_nd gp_entity_ptr(gp_person_t*  ) -> gp_entity_t*;

lu_fn_i_nd gp_person_ptr  (gp_entity_t*) -> gp_person_t*;
lu_fn_i_nd gp_building_ptr(gp_entity_t*) -> gp_building_t*;

lu_fn_i_nd gp_dist(const gp_entity_t&, const gp_entity_t&) -> f32;
lu_fn_i_nd gp_pos_terrain      (f32x2 pos_flat) -> i32x2;
lu_fn_i_nd gp_height_terrain   (f32x2 pos_flat) -> f32;
lu_fn_i_nd gp_pos_world        (const f32x2& pos_flat) -> f32x3;
lu_fn_i_nd gp_pos_world        (const gp_entity_t&) -> f32x3;
lu_fn_i_nd gp_orientation_world(const gp_entity_t&) -> rotor_f32x4;
lu_fn_i_nd gp_transform_of     (const gp_entity_t&) -> lu_transform;
lu_fn_i_nd gp_movement_speed   (f32x2 pos_flat, f32x2 dir_flat) -> f32;
lu_fn_i_nd gp_path_grid_cost   (i32x2 pos) -> f32;

lu_fn_i_nd gp_path_grid_pos          (f32x2 pos_flat) -> i32x2;
lu_fn_i_nd gp_pos_flat_from_path_grid(i32x2 pos) -> f32x2;
lu_fn_i_nd gp_path_grid_rect_from    (rect_f32x4 world_rect) -> rect_i32x4;
lu_fn_i_nd gp_path_grid_rect_from    (gp_entity_t* building_p) -> rect_i32x4;
lu_fn_i_nd gp_mouse_pos_flat         () -> f32x2;
lu_fn_i_nd gp_path_avg_dir_to        (f32x2 pos_flat, gp_entity_t* target_p) -> f32x2;

#include "game_data/gd_settings.h"
#include "game_data/gd_materials.h"
#include "game_data/gd_meshes.h"
#include "game_data/gd_visuals.h"
#include "game_data/gd_prefabs.h"
#include GM_LEVEL_FILE_PATH
#include "gameplay_ui.h"
#include "gameplay_draw.h"

///////////////
// implementation

lu_fn _gp_tick_eta(f32* eta_p) -> void;
lu_fn _gp_tick_and_reset_eta(f32* eta_p, f32 reset_value) -> bool; // returns true when cooldown elapsed

lu_fn gp_init() -> void {
    gp.           tick_profiler = lu_make_profiler(app.clock_measurer);
    gp.           draw_profiler = lu_make_profiler(app.clock_measurer);
    gp.update_blockage_profiler = lu_make_profiler(app.clock_measurer);
    gp.    pathfinding_profiler = lu_make_profiler(app.clock_measurer);
    gp.    tick_people_profiler = lu_make_profiler(app.clock_measurer);

    gd_make_settings    ();
    gd_make_materials   ();
    gd_make_meshes      ();
    gd_make_visuals     ();
    gp_make_prefabs     ();
    gd_make_lvl_island_0();

    gp.speed_sand    = 0.4f;
    gp.speed_grass   = 0.2f;

    gp.construction_approach_distance = 0.4f;
    gp.     scenery_approach_distance = 0.2f;

    gp.entity_types = {
        .scenery  = { .id = "scenery" , .avoidance = {1.5f, 0.5f}, .build_distance = 0.2f },
        .building = { .id = "building", .avoidance = {1.5f, 0.5f}, .build_distance = 0.4f },
        .person   = { .id = "person"  , .avoidance = {0.8f, 0.8f}, .build_distance = 0.0f },
    };

    gp.actions = {
        .take      = { .id = "take"     , .cooldown = 0.2f },
        .store     = { .id = "store"    , .cooldown = 0.2f },
        .construct = { .id = "construct", .cooldown = 1.0f },
        .cut_down  = { .id = "cut_down" , .cooldown = 1.0f },
        .chop      = { .id = "chop"     , .cooldown = 8.0f },
    };

    gp.job_types = {
        .delivery     = { .id = "delivery"     },
        .construction = { .id = "construction" },
        .woodcutting  = { .id = "woodcutting"  },
    };

    gp.res_types = {
        .log   = {.id = "log"  , .carried_visual = gd_vis.log_carried, .stored_visual = gd_vis.log_stored },
        .stone = {.id = "stone", .carried_visual = gd_vis.stone_carried, .stored_visual = gd_vis.stone_stored },
    };

    gp.grow_grass_cooldown = 3.0f;
    gp.people_remove_grass_cooldown = 0.01f;

    gp.pf_max_steps = gp_path_grid_area;
}

lu_fn gp_tick() -> void {
    // PERF_TODO: spread some calculations between frames
    lu_use_profiler(&gp.tick_profiler);

    if (!gp.paused); else return;
    if (gx.terrain_shape_initialized); else return;

    lu_block { // init terrain
        if (lu_change(&gp.terrain_initialized, true)); else continue;

        { // init terrain blockage map
            lu_for_rect(gp_path_grid_rect) {
                auto pos = _i32x2(x,y);
                gp.terrain_path_block_grid.set_fast(pos, gp_height_terrain(gp_pos_flat_from_path_grid(pos)) < 0.0f);
            }
        }

        { // init terrain materials
            lu_for_rect(gx_grid_rect)
                gp.grass_grid.set_fast(_i32x2(x, y), gx.terrain_materials.get_fast(x, y) == 255);
        }
    }

    lu_block { // update blockage grid
        lu_use_profiler(&gp.update_blockage_profiler);

        // copy terrain blockage
        gp.path_block_grid = gp.terrain_path_block_grid;

        // raster entities' blockage
        lu_for_arr(ent_i, gp.entities) {
            auto& ent = gp.entities[ent_i];
            auto& pf  = ent.prefab();
            if (pf.type == &gp.entity_types.scenery ); else
            if (pf.type == &gp.entity_types.building); else continue;

            auto rect = gp_path_grid_rect_from(_square(ent.position, pf.size));

            lu_for_rect(rect)
                gp.path_block_grid.set_fast(x, y);
        }
    }

    { // people ai
        lu_use_profiler(&gp.tick_people_profiler);

        lu_for_chain(person_p, gp.people) { // find and execute tasks
            auto& person     = person_p lu_deref;
            auto& person_ent = person.entity();

            person.approach_target = nullptr;

            auto proceed = true;
            lu_block { // perform
                auto& perform = person.task.perform;
                _gp_tick_eta(&perform.eta);
                proceed = perform.eta == 0.0f;
            }
            if (proceed); else continue;

            lu_block { // deliver
                auto& deliver = person.task.deliver;
                if (deliver.step); else continue;
                proceed = false;

                auto& from     = deliver.from     lu_deref;
                auto& to       = deliver.to       lu_deref;
                auto& res_type = deliver.res_type lu_deref;

                lu_block { // move to the storage and take the resource
                    if (deliver.step == 1); else continue;

                    if (person.approach_tick(&from));        else continue;
                    if (from.storage.any_stored(&res_type)); else continue;
                    if (person.perform(&gp.actions.take));   else continue;

                    from      .storage.sub(&res_type, 1);
                    person_ent.storage.add(&res_type, 1);
                    person_ent.storage.reserve(&res_type, 1);

                    deliver.step = 2;
                }

                lu_block { // move toward target 'to' and store the resource
                    if (deliver.step == 2); else continue;

                    if (person.approach_tick(deliver.to));  else continue;
                    if (person.perform(&gp.actions.store)); else continue;

                    person_ent.storage.sub(&res_type, 1);
                    to        .storage.add(&res_type, 1);

                    deliver = {};
                    proceed = true;
                }
            }
            if (proceed); else continue;

            lu_block { // gather
                auto& gather = person.task.gather;
                if (gather.step); else continue;

                auto& from     = gather.from     lu_deref;
                auto& res_type = gather.res_type lu_deref;

                // relying on the fact that 'deliver' is processed before 'gather'
                // therefore we won't check this condition before delivery is complete
                lu_assert(person.task.deliver.step == 0);

                proceed = false;

                lu_block { // cut down
                    if (gather.step == 1); else continue;

                    if (from.hit_points > 0); else continue;

                    if (person.approach_tick(&from)); else continue;
                    if (person.perform (&gp.actions.cut_down)); else continue;

                    from.hit_points -= 1;

                    if (from.hit_points == 0); else continue;

                    auto center_pos  = gp_pos_terrain(from.position);
                    auto rect = lu_clamp(_square(center_pos, 1), gx_grid_rect);
                    lu_for_rect(rect) {
                        auto pos = _i32x2(x,y);
                        auto& mat  = gx.terrain_materials[pos];
                        auto stamp = (i32)gx.trn_stamp[pos - center_pos + 1];
                        mat = (u8)lu_max(0, (i32)mat - stamp * 88);
                    }
                }

                lu_block { // 'from' is cut down, start chopping
                    if (gather.step == 1); else continue;
                    if (from.hit_points == 0); else continue;

                    if (from.structural_resources.try_reserve(&res_type, 1));
                        else {
                            // reset the task if there are no more resources left
                            gather = {};
                            continue;
                        }

                    from.storage.promise(&res_type, 1);

                    gather.step = 2;
                }

                lu_block { // chop
                    if (gather.step == 2); else continue;
                    lu_assert(from.hit_points == 0);

                    if (person.approach_tick(&from));      else continue;
                    if (person.perform(&gp.actions.chop)); else continue;

                    from.structural_resources.sub(&res_type, 1);
                    from.storage.add(&res_type, 1);

                    gather.step = 1;
                }
            }
            if (proceed); else continue;

            lu_block { // build
                auto& build = person.task.build;
                if (build.in_progress); else continue;

                auto& target = build.target lu_deref;
                lu_assert(target.prefab_->type == &gp.entity_types.building);
                lu_assert(target.building.is_under_construction());

                proceed = false;

                if (person.approach_tick(&target)); else continue;
                if (target.storage.any_stored(build.res_type)); else continue;
                if (person.perform(&gp.actions.construct)); else continue;

                build          .build_points -= 1;
                target.building.build_points -= 1;

                if (build.build_points == 0); else continue;

                // TODO: should I handle the case where there are multiple resources per build point?
                target.storage             .sub(build.res_type, 1);
                target.structural_resources.add(build.res_type, 1);

                lu_block { // finish the building
                    if (target.building.build_points == 0); else continue;

                    lu_assert(target.building.resources_remaining_for_build.none());
                    gp_finish_construction(&target.building);
                }

                build = {};
                proceed = true;
            }
            if (proceed); else continue;

            lu_block { // find a place to work
                if (!person.job); else continue;

                auto& job = gp.vacant_jobs.try_pop() lu_if_deref; else continue;
                lu_assert(!job.worker);
                job.worker = &person;
                person.job = &job;
                person.checked_in_at_workplace = false;
            }

            lu_block { // jobs
                auto& job = person.job lu_if_deref; else continue;
                auto& workplace     = job.workplace();
                auto& workplace_ent = workplace.entity();

                lu_block { // check in at the workplace first
                    if (!person.checked_in_at_workplace); else continue;
                    if ( person.approach_tick(&workplace_ent)); else continue;

                    person.checked_in_at_workplace = true;
                }

                if (person.checked_in_at_workplace); else continue;

                auto task_found = false;
                lu_block { // woodcutting
                    if (&job.type() == &gp.job_types.woodcutting); else continue;

                    auto gather_target_p = (gp_entity_t*)0;
                    lu_block { // find gather target
                        // TODO: Maybe guarantee a path exists by using workplace's path grid when searching for a resource?
                        auto min_dist = lu_large_f32;
                        lu_for_chain(sc_p, gp.sceneries) {
                            auto& source = sc_p lu_deref;

                            if (source.prefab().can_be_gathered); else continue;
                            if (source.structural_resources.any_available(&gp.res_types.log)); else continue;

                            auto dist = lu_dist(workplace_ent.position, source.position);
                            if (lu_update_min_value(&min_dist, dist))
                                gather_target_p = &source;
                        }
                    }

                    auto& target = gather_target_p lu_if_deref; else continue;

                    person.gather_start(&target, &gp.res_types.log);
                    task_found = true;
                }

                lu_block { // delivery
                    if (&job.type() == &gp.job_types.delivery); else continue;

                    lu_for_chain(building_p, gp.buildings_under_construction) {
                        if (!task_found); else break;

                        auto& building = building_p lu_deref;
                        auto& building_ent = building.entity();
                        auto& res_type = building.resources_remaining_for_build.first_type() lu_if_deref; else continue;

                        auto closest_provider_p = (gp_entity_t*)0;
                        auto min_dist = lu_large_f32;
                        lu_for_chain(provider_p, gp.resource_providers) {
                            auto& provider = provider_p lu_deref;

                            if (provider.storage.any_available(&res_type)); else continue;

                            auto dist = lu_dist(building_ent.position, provider.position);
                            if (lu_update_min_value(&min_dist, dist))
                                closest_provider_p = &provider;
                        }

                        lu_block {
                            auto& closest_provider = closest_provider_p lu_if_deref; else continue;
                            building.resources_remaining_for_build.sub(&res_type, 1);
                            person.deliver_start(&closest_provider, &building.entity(), &res_type);
                            task_found = true;
                            break;
                        }
                    }
                }

                lu_block { // construction
                    if (&job.type() == &gp.job_types.construction); else continue;

                    lu_for_chain(building_p, gp.buildings_under_construction) {
                        auto& building = building_p lu_deref;
                        auto& building_ent = building.entity();

                        if (building_ent.storage.any_available()); else continue;

                        person.build_start(&building_ent);
                        task_found = true;
                        break;
                    }
                }

                lu_block {
                    if (!task_found); else continue;
                    // NOTE: not blocking here, will continue to check for tasks each frame
                    person.approach_tick(&workplace_ent);
                }
            }

            lu_block { // no job, hang out at the town center
                if (!person.job); else continue;
                auto& town_center = gp.town_center lu_if_deref; else continue;

                person.approach_tick(&town_center.entity());
            }
        }

        lu_block { // pathfinding
            lu_use_profiler(&gp.pathfinding_profiler);

            lu_for_chain(target_p, gp.pathfinding_targets_chain) {
                auto& target = target_p lu_deref;

                lu_for_chain(pathfinder_p, target.pathfinders_chain) {
                    auto& pathfinder = pathfinder_p lu_deref;
                    pathfinder.pf_cost_to_goal = lu_large_f32;
                }

                auto& path_roots_grid = target.path_grid lu_deref;
                path_roots_grid.reset();

                auto cost_to_goal_grid   = lu_grid   <f32,       gp_path_grid_size> {};
                auto processed_grid      = lu_grid_bit_4x4<      gp_path_grid_size> {};
                auto path_priority_queue = lu_minheap<f32, u8x2, gp_path_grid_area> {};

                auto goal_rect = gp_path_grid_rect_from(&target);

                lu_for_rect(goal_rect) {
                    auto pos_i32 = _i32x2(x,y);
                    auto pos_u8  =  _u8x2(x,y);
                    path_roots_grid.set_fast(pos_i32, pos_u8);
                    // we set the initial goal's cost to 1.0f to avoid collision with the default value of 0.0f
                    // we could use an extra data bit grid, but that might slow down the calculations
                    // also maybe use some other value, to increase precision (likely not needed)
                    cost_to_goal_grid  . set_fast(pos_i32, 1.0f);
                    path_priority_queue.push_fast(1.0f,  pos_u8);
                }

                //   0
                // 3 * 1
                //   2
                const i8x2 deltas[] = {
                    _i8x2( 0,  1),
                    _i8x2( 1,  0),
                    _i8x2( 0, -1),
                    _i8x2(-1,  0),
                };

                auto max_cost = lu_large_f32;
                auto processed_i = 0;
                while (path_priority_queue.count > 0 && processed_i < gp.pf_max_steps) {
                    auto [node_to_goal_cost, node_pos_u8x2] = path_priority_queue.pop_fast();
                    auto node_pos = _i32x2(node_pos_u8x2);

                    // we can schedule the same node multiple times, but only the one with the lowest cost gets processed
                    if (!processed_grid.get_and_set_fast(node_pos)); else continue;

                    max_cost = 0.0f;
                    lu_for_chain(pathfinder_p, target.pathfinders_chain) {
                        auto& pathfinder = pathfinder_p lu_deref;
                        auto& ent        = pathfinder.entity();
                        auto  ent_pos    = gp_path_grid_pos(ent.position);

                        if (ent_pos == node_pos)
                            pathfinder.pf_cost_to_goal = node_to_goal_cost;

                        lu_update_max_value(&max_cost, pathfinder.pf_cost_to_goal);
                    }

                    if (node_to_goal_cost <= max_cost); else break;

                    if (true
                        && node_pos.x > 0
                        && node_pos.x < gp_path_grid_size.w - 1
                        && node_pos.y > 0
                        && node_pos.y < gp_path_grid_size.h - 1
                    ); else continue;

                    auto node_root      = _i32x2(path_roots_grid.get_fast(node_pos));
                    auto node_unit_cost = gp_path_grid_cost(node_pos);

                    lu_for_count(neigh_i, lu_arr_length(deltas)) {
                        auto node_to_neigh = _i32x2(deltas[neigh_i]);
                        auto neigh_pos = node_pos + node_to_neigh;
                        if (!processed_grid.get_fast(neigh_pos)); else continue;

                        // by construction, the fastest path to 'neigh_pos' is through 'pos',
                        //     otherwise we would've arrived here earlier

                        auto neigh_unit_cost         =  gp_path_grid_cost(neigh_pos);
                        auto neigh_to_node_root_sign =  lu_sign(lu_from_to(neigh_pos, node_root));
                        auto node_to_neigh_sign      = -lu_sign(node_to_neigh);

                        auto neigh_root = node_root;

                        lu_block { // find the new root
                            if (node_unit_cost != neigh_unit_cost) {
                                neigh_root = node_pos;
                                continue;
                            }

                            // check the cell intersected by the line to root for different cost and another path intersection
                            lu_for_count(axis, 2) {
                                auto other_axis = (axis + 1) % 2;
                                if (node_to_neigh_sign     [      axis]); else continue;
                                if (neigh_to_node_root_sign[other_axis]); else continue;

                                auto offset = i32x2_zero;
                                offset[other_axis] = neigh_to_node_root_sign[other_axis];

                                // NOTE: check_pos might not be processed at this point (e.g. if it's blocked)
                                auto check_pos = neigh_pos + offset;
                                auto check_unit_cost = gp_path_grid_cost(check_pos);

                                // check for different cost
                                if (check_unit_cost != neigh_unit_cost) {
                                    neigh_root = node_pos;
                                    break;
                                }

                                auto check_root = path_roots_grid.get_fast(check_pos);

                                // check for intersection with another path
                                if (check_root != u8x2_zero
                                &&  lu_do_segments_intersect(
                                    _f32x2(check_pos), _f32x2(check_root),
                                    _f32x2(neigh_pos), _f32x2( node_root)
                                )) {
                                    neigh_root = node_pos;
                                    break;
                                }
                            }
                        }

                        { // calculate the cost and enqueue
                            // we create a root on every cost change, so the cost to root is just length * cost per unit
                            auto neigh_to_root_dist = lu_length(_f32x2(neigh_pos - neigh_root));
                            auto neigh_to_goal_cost = cost_to_goal_grid.get_fast(neigh_root) + neigh_unit_cost * neigh_to_root_dist;

                            auto& min_cost_to_goal = cost_to_goal_grid[neigh_pos];
                            // 0.0f is the default value, meaning the node wasn't encountered yet
                            if (min_cost_to_goal == 0.0f); else
                            if (neigh_to_goal_cost < min_cost_to_goal); else continue;

                            min_cost_to_goal = neigh_to_goal_cost;
                            path_roots_grid.set_fast(neigh_pos, _u8x2(neigh_root));

                            path_priority_queue.push_fast(neigh_to_goal_cost, _u8x2(neigh_pos));
                        }
                    }

                    gp.pf_last_node = node_pos;
                    processed_i++;
                }

                target.pathfinders_chain.clear();
            }

            gp.pathfinding_targets_chain.clear();
        }

        lu_for_chain(person_p, gp.people) { // movement
            auto& person = person_p lu_deref;

            // not moving while performing an action
            if (person.task.perform.eta == 0.0f); else continue;

            auto& person_ent = person.entity();
            auto& person_pos = person_ent.position;

            auto dir = f32x2_zero;
            auto& target = person.approach_target lu_if_deref
                dir = gp_path_avg_dir_to(person_pos, &target);

            lu_block { // avoid neighbours
                auto steer = f32x2_zero;
                lu_for_chain(other_p, gp.people) {
                    auto& other = other_p lu_deref;
                    if (&person != &other); else continue;
                    auto& other_ent = other.entity();

                    auto offset = person_pos - other_ent.position;
                    auto dist   = lu_length(offset);

                    auto avoidance = gp.entity_types.person.avoidance;
                    auto rng = avoidance.range * other_ent.prefab().size;
                    auto w   = avoidance.weight;
                    if (w > 0.0f && rng > 0.0f); else continue;

                    // direction weighed by distance
                    steer += dist > lu_epsilon
                        ? offset * (w * lu_clamp_unorm((rng - dist) / rng) / dist)
                        : _f32x2(0,-1) // this should depend on entity id
                    ;
                }

                dir += steer;
            }

            lu_block { // apply motion
                if (dir != f32x2_zero); else continue;

                auto length = lu_length(dir);
                auto ndir = dir / length;

                auto speed = gp_movement_speed(person_pos, ndir);

                if (person_ent.storage.any_stored())
                    speed *= 0.75f;

                person_pos += ndir * lu_min(0.5f + length, 1.0f) * (speed * (f32)lgl_time.game_delta_time);
                person.target_orientation = _rotor_f32x2_from_to(_f32x2(0, -1), ndir);

                lu_assert(person.target_orientation.is_valid())
            }

            { // rotate to target orientation
                auto& person_rot = person_ent.orientation;
                person_rot = lu_slerp(person_rot, person.target_orientation, 0.2f);
            }
        }
    }

    { // grass
        lu_block { // people stomp grass
            if (_gp_tick_and_reset_eta(&gp.people_remove_grass_eta, gp.people_remove_grass_cooldown)); else continue;
            lu_for_chain(person_p, gp.people) {
                auto& person = person_p lu_deref;
                auto& mat = gx.terrain_materials[gp_pos_terrain(person.entity().position)];
                mat = (u8)lu_clamp((i32)mat - 2, 0, 255);
            }
        }

        lu_block { // grow grass
            if (_gp_tick_and_reset_eta(&gp.grow_grass_eta, gp.grow_grass_cooldown)); else continue;

            lu_for_rect(gx_grid_rect) {
                if (gp.grass_grid.get_fast(x, y)); else continue;
                auto& mat = gx.terrain_materials[x, y];
                mat = (u8)lu_min((i32)mat + 1, 255);
            }
        }
    }
}

lu_fn gp_make_entity(const gp_entity_t& spec) -> gp_entity_t& {
    auto& ent = gp.entities.push_fast(spec);
    auto& pf  = ent.prefab();
    lu_block { // init scenery
        if (ent.prefab().type == &gp.entity_types.scenery); else continue;

        gp.sceneries.push(&ent);

        ent.hit_points = pf.max_hit_points;
        ent.path_grid  = &gp.path_grids.push_fast();

        ent.structural_resources.add_immediate(pf.max_inner_resources);

        lu_block { // init resources
            if (pf.provides_resources); else continue;
            gp.resource_providers.push(&ent);
        }
    }

    lu_block { // init building
        auto& building = gp_building_ptr(&ent) lu_if_deref; else continue;

        gp.buildings.push(&building);
        ent.path_grid = &gp.path_grids.push_fast();

        lu_block { // init construction
            if (building.is_under_construction()); else continue;

            building.resources_remaining_for_build = pf.resources_for_construction;
            gp.buildings_under_construction.push(&building);
        }

        lu_block { // finish construction
            if (!building.is_under_construction()); else continue;
            gp_finish_construction(&building);
        }
    }

    lu_block { // init person
        auto& person = gp_person_ptr(&ent) lu_if_deref; else continue;
        gp.people.push(&person);
    }

    return ent;
}

lu_fn gp_build(const char* name, gp_entity_prefab_t* bp_p, f32x2 position, rotor_f32x2 orientation) -> gp_building_t& {
    auto& pf = bp_p lu_deref;
    return gp_make_entity({
        .id = name,
        .prefab_     = &pf,
        .position    = position,
        .orientation = orientation,
        .building = {
            .build_points = pf.max_build_points,
        },
        .is_dynamic = true,
    }).building;
}

lu_fn gp_finish_construction(gp_building_t* building_p) -> void {
    auto& building = building_p lu_deref;
    auto& ent      = building.entity();
    auto& pf       = ent.prefab();
    lu_assert(!building.is_under_construction());

    gp.buildings_under_construction.remove(&building);

    lu_block { // init town center
        if (pf.is_town_center); else continue;
        lu_assert(!gp.town_center);
        gp.town_center = building_p;
    }

    lu_block { // init resources
        if (pf.provides_resources); else continue;
        gp.resource_providers.push(&ent);
    }

    lu_block { // init jobs
        if (pf.job_types.count); else continue;
        building.jobs.splat_zero_fast(pf.job_types.count);

        lu_for_arr_inv(ji, building.jobs)
            gp.vacant_jobs.push_front(&building.jobs[ji]);
    }
}

lu_fn_i_nd gp_ent_id(gp_entity_t* p) -> u16 { return p ? (u16)gp.entities.index_of(p) + 1 : (u16)0; }

lu_fn_i_nd gp_entity_ptr(gp_building_t* bp) -> gp_entity_t* {
    lu_assert(gp.entities.item_ptr_from_field(bp));
    return (gp_entity_t*)((uptr)bp - offsetof(gp_entity_t, building));
}

lu_fn_i_nd gp_entity_ptr(gp_person_t* pp) -> gp_entity_t* {
    lu_assert(gp.entities.item_ptr_from_field(pp));
    return (gp_entity_t*)((uptr)pp - offsetof(gp_entity_t, person));
}

lu_fn_i_nd gp_person_ptr(gp_entity_t* ent_p) -> gp_person_t* {
    auto& ent = ent_p lu_if_deref; else return nullptr;
    auto& pf  = ent.prefab();
    return pf.type == &gp.entity_types.person ? &ent.person : nullptr;
}
lu_fn_i_nd gp_building_ptr(gp_entity_t* ent_p) -> gp_building_t* {
    auto& ent = ent_p lu_if_deref; else return nullptr;
    auto& pf  = ent.prefab();
    return pf.type == &gp.entity_types.building ? &ent.building : nullptr;
}

lu_fn_i_nd gp_dist(const gp_entity_t& one, const gp_entity_t& other) -> f32 {
    return lu_dist(one.position, other.position);
}

lu_fn_i_nd gp_pos_terrain(f32x2 pos_flat) -> i32x2 {
    return _i32x2(_f32x2(gx_grid_size.vec) * ((pos_flat / gp_world_size.vec) + 0.5f));
}

lu_fn_i_nd gp_height_terrain(f32x2 pos_flat) -> f32 {
    auto pos = gp_pos_terrain(pos_flat);
    if (lu_is_within(pos, gx_grid_rect)); else return -GM_WORLD_SIZE_Z * 0.5f;
    return gx.terrain_heightmap.get_fast(pos) + 2.0f * GM_VOX_TO_WORLD;
}

lu_fn_i_nd gp_pos_world(const f32x2& pos_flat) -> f32x3 {
    return _f32x3(pos_flat, gp_height_terrain(pos_flat));

}
lu_fn_i_nd gp_pos_world(const gp_entity_t& entity) -> f32x3 {
    return gp_pos_world(entity.position);
}

lu_fn_i_nd gp_orientation_world(const gp_entity_t& entity) -> rotor_f32x4 {
    return _rotor_f32x4_xy(entity.orientation);
}

lu_fn_i_nd gp_transform_of(const gp_entity_t& entity) -> lu_transform {
    return {
        .position    = gp_pos_world(entity),
        .orientation = gp_orientation_world(entity),
        .scale       = f32x3_unit
    };
}

lu_fn_i_nd gp_movement_speed(f32x2 pos_flat, f32x2 dir_flat) -> f32 {
    auto terrain_pos  = gp_pos_terrain(pos_flat);
    auto terrain_speed = (_unorm(gx.terrain_materials.get_fast(terrain_pos)) > 0.45f ? gp.speed_grass : gp.speed_sand);

    lu_assert(lu_abs(lu_length(dir_flat) - 1.0f) < lu_epsilon_coarse);

    auto height = gx.terrain_heightmap[terrain_pos];

    auto height_diff = _f32x2(
        gx.terrain_heightmap[terrain_pos + _i32x2(1, 0)] - height,
        gx.terrain_heightmap[terrain_pos + _i32x2(0, 1)] - height
    );

    return terrain_speed * (1.0f - 0.5f * lu_dot(dir_flat, height_diff) / GM_WORLD_SIZE_Z);
}

lu_fn_i_nd gp_path_grid_cost(i32x2 pos) -> f32 {
    auto   world_pos = gp_pos_flat_from_path_grid(pos);
    auto terrain_pos = gp_pos_terrain(world_pos);
    return gp.path_block_grid.get_fast(gp_path_grid_pos(world_pos))
        ? 8.0f
        : (_unorm(gx.terrain_materials.get_fast(terrain_pos)) > 0.45f ? 1.0f : 0.5f);
}

lu_fn_i_nd gp_path_grid_pos(f32x2 pos_flat) -> i32x2 {
    return _i32x2(lu_round((pos_flat / gp_world_size.vec + 0.5f) * _f32x2(gp_path_grid_max_pos)));
}

lu_fn_i_nd gp_path_grid_rect_from(rect_f32x4 world_rect) -> rect_i32x4 {
    auto rect = _rect(gp_path_grid_pos(world_rect.tl), gp_path_grid_pos(world_rect.br));
    rect.tl = lu_clamp(rect.tl, i32x2_zero, gp_path_grid_max_pos);
    rect.br = lu_clamp(rect.br, i32x2_zero, gp_path_grid_max_pos);
    return rect;
}

lu_fn_i_nd gp_path_grid_rect_from(gp_entity_t* ent_p) -> rect_i32x4 {
    auto& ent = ent_p lu_deref;
    auto& pf = ent.prefab();
    lu_block {
        auto& building = gp_building_ptr(&ent) lu_if_deref; else continue;
        if (!building.is_under_construction()); else continue;
        return gp_path_grid_rect_from(_square(ent.get_approach_pos(), 0.0f));
    }

    return gp_path_grid_rect_from(_square(ent.position, pf.size));
}

lu_fn_i_nd gp_pos_flat_from_path_grid(i32x2 pos) -> f32x2 {
    return ((_f32x2(pos) / _f32x2(gp_path_grid_max_pos)) - 0.5f) * gp_world_size.vec;
}

lu_fn_i_nd gp_mouse_pos_flat() -> f32x2 {
    return lgx_mouse_ray_intersect(gx.camera, {
        .origin = _f32x3(0, 0, 0.2f),
        .normal = _f32x3(0, 0, 1),
    }).xy;
}

lu_fn_i_nd gp_path_avg_dir_to(f32x2 pos_flat, gp_entity_t* target_p) -> f32x2 {
    lu_const movement_smoothing_radius = 0.1f;

    auto& target = target_p lu_deref;
    auto dir = f32x2_zero;

    lu_block {
        auto& path_roots_grid = target.path_grid lu_if_deref; else continue;

        auto grid_pos  = gp_path_grid_pos(pos_flat);
        auto main_root = path_roots_grid.get(grid_pos);
        if (main_root != u8x2_zero); else continue;

        //auto main_dir = gp_pos_flat_from_path_grid(_i32x2(main_root)) - pos_flat;

        //// if the main pos is within a blocked cell, use main_dir only
        //if (!gp.path_block_grid[grid_pos]);
        //    else {
        //        dir = main_dir;
        //        break;
        //    }

        lu_for_rect(gp_path_grid_rect_from(_square(pos_flat, movement_smoothing_radius))) {
            auto next_grid_pos = _i32x2(x, y);
            auto next_root = path_roots_grid.get(next_grid_pos);
            if (next_root != u8x2_zero); else continue;
            auto pos_to_root = lu_from_to(pos_flat, gp_pos_flat_from_path_grid(_i32x2(next_root)));
            //if (lu_dot(main_dir, root_dir) > 0); else continue;
            auto dist = lu_dist(gp_pos_flat_from_path_grid(next_grid_pos), pos_flat);
            dir += pos_to_root / (dist * (1.0f + dist));
        }
    }

    if (dir != f32x2_zero)
        dir = lu_normalize(dir);
    else
        dir = lu_direction(pos_flat, target.get_approach_pos());

    return dir;
}

lu_mt gp_person_t::approach_tick(gp_entity_t* target_p) -> bool { // -> finished
    auto& ent       = entity();
    auto& target    = target_p lu_deref;
    auto& target_pf = target.prefab();

    auto approach_distance = 0.1f;
    if (target_pf.type == &gp.entity_types.building) {
        if (target.building.is_under_construction())
            approach_distance = gp.construction_approach_distance;
    }
    else if (target_pf.type == &gp.entity_types.scenery)
        approach_distance = gp.scenery_approach_distance;

    if (lu_dist(ent.position, target.get_approach_pos()) < approach_distance) {
        approach_target = nullptr;
        return true;
    }

    lu_block { // schedule pathfinding
        target.pathfinders_chain.push(this);
        lu_block {
            // pathfinding for this target is not scheduled yet
            if (!target.pathfinding_target_link.is_within_chain()); else continue;
            gp.pathfinding_targets_chain.push(&target);
        }
    }

    approach_target = &target;

    return false;
}

lu_mt gp_person_t::perform(gp_action_t* action_p) -> bool { // -> finished
    auto& action  = action_p lu_deref;
    auto& perform = task.perform;
    lu_block {
        if (perform.action != &action); else continue;
        task.perform = {
            .action = &action,
            .eta = action.cooldown
        };
    }

    auto result = task.perform.eta == 0.0f;
    if (result)
        task.perform = {};

    return result;
}

lu_mt gp_person_t::deliver_start(gp_entity_t* from_p, gp_entity_t* to_p, gp_resource_type* resource_type_p) -> void {
    auto& deliver = task.deliver;
    lu_assert(!deliver.step);

    auto& from = from_p lu_deref;
    auto& to   = to_p   lu_deref;

    deliver = {
        .from = from_p,
        .to   = to_p,
        .res_type = resource_type_p,

        .step = 1,
    };

    from    .storage.reserve(resource_type_p, 1);
    to      .storage.promise(resource_type_p, 1);
    entity().storage.promise(resource_type_p, 1);
}

lu_mt gp_person_t::build_start(gp_entity_t* target_p) -> void {
    auto& build = task.build;
    lu_assert(!build.in_progress);

    auto& target   = target_p lu_deref;
    auto& building = target.building;
    auto& res_type = target.storage.first_available_type() lu_deref;

    auto& bp = target.prefab();
    auto total_resources = bp.resources_for_construction.total();

    lu_block { // first build batch
        if (building.build_points == bp.max_build_points); else continue;
        building.build_points_error = 2 * (i32)(bp.max_build_points % total_resources) - (i32)total_resources;
    }

    target.storage.reserve(&res_type, 1);
    target.structural_resources.promise(&res_type, 1);

    auto bp_amount = bp.max_build_points / total_resources;

    auto error_correction = 2 * (i32)(bp.max_build_points % total_resources);
    if (building.build_points_error > 0) {
        bp_amount        += 1;
        error_correction -= 2 * total_resources;
    }
    building.build_points_error += error_correction;

    build = {
        .target       = target_p,
        .res_type     = &res_type,
        .build_points = bp_amount,

        .in_progress = true,
    };
}

lu_mt gp_person_t::gather_start(gp_entity_t* from_p, gp_resource_type* res_type_p) -> void {
    auto& gather = task.gather;
    lu_assert(!gather.step);

    gather = {
        .from     = from_p,
        .res_type = res_type_p,

        .step = 1,
    };
}

lu_mt_nd gp_entity_t::get_approach_pos() const  -> f32x2 {
    auto& pf = prefab();
    auto approach_pos_flat = position;

    if (pf.type == &gp.entity_types.building && !building.is_under_construction())
        approach_pos_flat += pf.entrance_offset >> orientation;

    return approach_pos_flat;
}

lu_mt_nd gp_building_t::entity() -> gp_entity_t& { return *gp_entity_ptr(this); }
lu_mt_nd gp_person_t  ::entity() -> gp_entity_t& { return *gp_entity_ptr(this); }

lu_fn _gp_tick_eta(f32* eta_p) -> void {
    auto& eta = eta_p lu_deref;
    eta = lu_max(0.0f, eta - (f32)lgl_time.game_delta_time);
}

lu_fn _gp_tick_and_reset_eta(f32* eta_p, f32 reset_value) -> bool {
    auto& eta = eta_p lu_deref;
    _gp_tick_eta(eta_p);
    auto elapsed = eta == 0.0f;
    if (elapsed)
        eta = reset_value;
    return elapsed;
}

lu_mt_i_nd gp_job_t::type() -> gp_job_type_t& {
    auto& wp = workplace();
    auto index = wp.jobs.index_of(this);
    return *wp.entity().prefab().job_types[index];
}

lu_mt_i_nd gp_job_t::workplace() -> gp_building_t& {
    auto& entity = gp.entities.item_ptr_from_field(this) lu_deref;
    return entity.building;
}
