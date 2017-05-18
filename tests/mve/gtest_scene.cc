#include <gtest/gtest.h>

#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>
#include <chrono>
#include <thread>

#include "mve/bundle_io.h"
#include "mve/scene.h"
#include "util/file_system.h"

namespace {

/* Cleanup mechanism. Unlinks files on destruction. */
class OnScopeExit final
{
public:
    ~OnScopeExit();
    void unlink(std::string str);

private:
    void unlink_recursive(const std::string& str);

    std::vector<std::string> paths;
};

/**
 * Creates a valid scene directory in the system's temporary directory.
 * This directory has an empty "views" subdirectory and
 * a bundle file with zero cameras and zero features.
 * @param view_count Number of views the scene is created with.
 * @param bundle Bundle to include within the scene or
 *     nullptr to create a scene without a bundle file.
 * @return The path to the created scene directory.
 */
std::string create_scene_on_disk(const std::size_t view_count,
                                 const mve::Bundle::Ptr bundle,
                                 OnScopeExit& on_scope_exit);

void make_dirty(mve::View::Ptr view);
void make_a_clean_view_dirty(mve::Scene::Ptr scene);

OnScopeExit on_global_scope_exit;

} // anonymous namespace

TEST(SceneTest,
     a_created_scene_is_initialy_clean)
{
    OnScopeExit clean_up;

    std::string scene_directory = create_scene_on_disk(0, nullptr, clean_up);
    mve::Scene::Ptr scene = mve::Scene::create(scene_directory);
    ASSERT_TRUE(scene != nullptr);
    EXPECT_FALSE(scene->is_dirty());
}

TEST(SceneTest,
     the_view_count_of_a_created_scene_matches_with_that_scene_on_disk)
{
    OnScopeExit clean_up;

    mve::Scene::Ptr scene_without_views = [&clean_up](){
        std::string path_to_scene = create_scene_on_disk(0, nullptr, clean_up);
        return mve::Scene::create(path_to_scene);
    }();
    EXPECT_EQ(std::size_t{0}, scene_without_views->get_views().size());

    mve::Scene::Ptr scene_with_10_views = [&clean_up](){
        std::string path_to_scene = create_scene_on_disk(10, nullptr, clean_up);
        return mve::Scene::create(path_to_scene);
    }();
    EXPECT_EQ(std::size_t{10}, scene_with_10_views->get_views().size());
}

TEST(SceneTest,
     a_clean_scene_becomes_dirty_if_any_of_its_views_become_dirty)
{
    OnScopeExit clean_up;

    mve::Scene::Ptr clean_scene = [&clean_up](){
        std::string path_to_scene = create_scene_on_disk(5, nullptr, clean_up);
        return mve::Scene::create(path_to_scene);
    }();
    make_a_clean_view_dirty(clean_scene);
    EXPECT_TRUE(clean_scene->is_dirty());
}

TEST(SceneTest,
     set_bundle_makes_a_clean_scene_dirty)
{
    OnScopeExit clean_up;

    mve::Scene::Ptr clean_scene = [&clean_up](){
        std::string path_to_scene = create_scene_on_disk(5,
                                                         mve::Bundle::create(),
                                                         clean_up);
        return mve::Scene::create(path_to_scene);
    }();
    clean_scene->set_bundle(mve::Bundle::create());
    EXPECT_TRUE(clean_scene->is_dirty());
}

TEST(SceneTest,
     a_dirty_scene_remains_dirty_when_more_of_its_elements_become_dirty)
{
    OnScopeExit clean_up;

    mve::Scene::Ptr dirty_scene = [&clean_up]() {
        std::string path_to_scene = create_scene_on_disk(7,
                                                         mve::Bundle::create(),
                                                         clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(path_to_scene);
        make_a_clean_view_dirty(scene);
        return scene;
    }();
    make_a_clean_view_dirty(dirty_scene);
    EXPECT_TRUE(dirty_scene->is_dirty());

    dirty_scene->set_bundle(mve::Bundle::create());
    EXPECT_TRUE(dirty_scene->is_dirty());
}

TEST(SceneTest,
     saving_a_dirty_scene_cleans_it)
{
    OnScopeExit clean_up;

    mve::Scene::Ptr dirty_scene = [&clean_up]() {
        std::string path_to_scene = create_scene_on_disk(7, nullptr, clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(path_to_scene);
        make_a_clean_view_dirty(scene);
        return scene;
    }();
    dirty_scene->save_scene();
    EXPECT_FALSE(dirty_scene->is_dirty());
}

TEST(SceneTest,
     save_views_cleans_a_scene_if_only_its_views_are_dirty)
{
    OnScopeExit clean_up;

    mve::Scene::Ptr scene_with_dirty_views_and_clean_bundle = [&clean_up]() {
        std::string path_to_scene = create_scene_on_disk(7, nullptr, clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(path_to_scene);
        for(int i = 0; i < 7; ++i)
            make_a_clean_view_dirty(scene);
        return scene;
    }();
    scene_with_dirty_views_and_clean_bundle->save_views();
    EXPECT_FALSE(scene_with_dirty_views_and_clean_bundle->is_dirty());
}

TEST(SceneTest,
     save_views_does_not_clean_a_scene_if_its_bundle_is_dirty)
{
    OnScopeExit clean_up;

    mve::Scene::Ptr scene_with_dirty_bundle = [&clean_up]() {
        std::string path_to_scene = create_scene_on_disk(7, nullptr, clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(path_to_scene);
        for(int i = 0; i < 7; ++i)
            make_a_clean_view_dirty(scene);
        scene->set_bundle(mve::Bundle::create());
        return scene;
    }();
    scene_with_dirty_bundle->save_views();
    EXPECT_TRUE(scene_with_dirty_bundle->is_dirty());
}

TEST(SceneTest,
     save_bundle_cleans_a_scene_if_only_its_bundle_is_dirty)
{
    OnScopeExit clean_up;

    mve::Scene::Ptr scene_with_dirty_bundle_and_clean_views = [&clean_up]() {
        std::string path_to_scene = create_scene_on_disk(7, nullptr, clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(path_to_scene);
        scene->set_bundle(mve::Bundle::create());
        return scene;
    }();
    scene_with_dirty_bundle_and_clean_views->save_bundle();
    EXPECT_FALSE(scene_with_dirty_bundle_and_clean_views->is_dirty());
}

TEST(SceneTest,
     save_bundle_does_not_clean_a_scene_if_any_of_its_views_is_dirty)
{
    OnScopeExit clean_up;

    mve::Scene::Ptr scene_with_dirty_view = [&clean_up]() {
        std::string path_to_scene = create_scene_on_disk(7, nullptr, clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(path_to_scene);
        make_a_clean_view_dirty(scene);
        scene->set_bundle(mve::Bundle::create());
        return scene;
    }();
    scene_with_dirty_view->save_views();
    EXPECT_TRUE(scene_with_dirty_view->is_dirty());
}

TEST(SceneTest,
     saving_the_dirty_views_of_a_scene_cleans_the_scene_if_its_bundle_is_clean)
{
    OnScopeExit clean_up;
    mve::Scene::Ptr scene_with_dirty_views_and_clean_bundle = [&clean_up]() {
        std::string path_to_scene = create_scene_on_disk(10,
                                                         mve::Bundle::create(),
                                                         clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(path_to_scene);
        for(int i = 0; i < 5; ++i)
            make_a_clean_view_dirty(scene);
        return scene;
    }();
    using View = mve::View::Ptr;
    for (View& view : scene_with_dirty_views_and_clean_bundle->get_views())
        if (view->is_dirty())
            view->save_view();
    EXPECT_FALSE(scene_with_dirty_views_and_clean_bundle->is_dirty());
}

namespace {

std::string
create_scene_on_disk(const std::size_t view_count,
                     const mve::Bundle::Ptr bundle,
                     OnScopeExit& on_scope_exit)
{
    namespace fs = util::fs;

    std::string scene_directory { std::tmpnam(nullptr) };
                scene_directory += "_test_scene";
    std::string bundle_file = fs::join_path(scene_directory, "synth_0.out");
    std::string views_directory = fs::join_path(scene_directory, "views");

    fs::mkdir(scene_directory.c_str());
    on_scope_exit.unlink(scene_directory); // Schedules for cleanup.
    fs::mkdir(views_directory.c_str());

    for (std::size_t i = 0; i < view_count; ++i)
    {
        const std::string view_directory_path = [&i, &views_directory](){
            std::stringstream stream;
            stream << fs::join_path(views_directory, "view_")
                   << std::setw(4) << std::setfill('0') << std::to_string(i)
                   << ".mve";
            return stream.str();
        }();
        fs::mkdir(view_directory_path.c_str());
        mve::View::Ptr view = mve::View::create();
        view->set_name("view" + std::to_string(i));
        view->set_id(i);
        view->save_view_as(view_directory_path);
    }

    if(bundle != nullptr)
        mve::save_mve_bundle(bundle, bundle_file);

    return scene_directory;
}

void make_dirty(mve::View::Ptr view)
{
    view->set_name(view->get_name() + 'a');
    assert(view->is_dirty());
}

void make_a_clean_view_dirty(mve::Scene::Ptr scene)
{
    using ViewList = mve::Scene::ViewList;
    ViewList& views = scene->get_views();
    ViewList::iterator clean_view = std::find_if(views.begin(), views.end(),
        [](mve::View::Ptr& view) { return !view->is_dirty(); });
    make_dirty(*clean_view);
}

OnScopeExit::~OnScopeExit()
{
    bool exception_happened = false;
    for(std::string path : this->paths)
    {
        try
        {
            this->unlink_recursive(path);
        } catch (...)
        {
            exception_happened = true;
            std::cout <<  "Exception during cleanup!" << std::endl;
        }
    }
    if (exception_happened)
        throw std::runtime_error("Exception during file cleanup.");
}

void OnScopeExit::unlink(std::string str)
{
    this->paths.push_back(std::move(str));
}

void OnScopeExit::unlink_recursive(const std::string& path)
{
    namespace fs = util::fs;
    if(fs::file_exists(path.c_str()))
    {
       fs::unlink(path.c_str());
    }
    else if(fs::dir_exists(path.c_str()))
    {
        fs::Directory directory(path);
        for(const fs::File& node : directory)
            unlink_recursive(node.get_absolute_name());
        fs::rmdir(path.c_str());
    }
}

} // anonymous namespace