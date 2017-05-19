#include <gtest/gtest.h>

#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>

#include "mve/bundle_io.h"
#include "mve/scene.h"
#include "util/exception.h"
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
                                 OnScopeExit& clean_up);

void make_dirty(mve::View::Ptr view);
void make_a_clean_view_dirty(mve::Scene::Ptr scene);
mve::Scene::ViewList
        load_views_directly_from(const std::string& scene_directory);
mve::Bundle::Ptr
        load_bundle_directly_from(const std::string& scene_directory);
mve::Bundle::Ptr make_bundle(const std::size_t camera_count);
bool views_match(mve::Scene::ViewList& lhs, mve::Scene::ViewList& rhs);
bool bundle_cameras_match(mve::Bundle::Ptr lhs, mve::Bundle::ConstPtr rhs);

} // anonymous namespace

//== Test the initial state of a created scene =================================

TEST(SceneTest,
     a_created_scene_is_initialy_clean)
{
    OnScopeExit clean_up;

    std::string scene_path = create_scene_on_disk(0, nullptr, clean_up);
    mve::Scene::Ptr scene = mve::Scene::create(scene_path);
    ASSERT_TRUE(scene != nullptr);
    EXPECT_FALSE(scene->is_dirty());
}

TEST(SceneTest,
     the_initial_path_of_a_created_scene_is_the_path_it_was_created_with)
{
    OnScopeExit clean_up;

    std::string scene_path = create_scene_on_disk(0, make_bundle(0), clean_up);
    mve::Scene::Ptr scene = mve::Scene::create(scene_path);
    EXPECT_EQ(scene_path, scene->get_path());
}

TEST(SceneTest,
     the_initial_views_of_a_created_scene_match_with_that_scene_on_disk)
{
    using ViewList = mve::Scene::ViewList;
    OnScopeExit clean_up;

    mve::Scene::Ptr scene_without_views = [&clean_up](){
        std::string path_to_scene =
                create_scene_on_disk(0, make_bundle(5), clean_up);
        return mve::Scene::create(path_to_scene);
    }();
    EXPECT_EQ(std::size_t{0}, scene_without_views->get_views().size());

    mve::Scene::Ptr scene_with_many_views = [&clean_up](){
        std::string path_to_scene =
                create_scene_on_disk(73, make_bundle(23), clean_up);
        return mve::Scene::create(path_to_scene);
    }();
    ViewList views_on_disk =
            load_views_directly_from(scene_with_many_views->get_path());
    EXPECT_TRUE(views_match(views_on_disk,
                            scene_with_many_views->get_views()));
}

TEST(SceneTest,
     the_initial_bundle_of_a_created_scene_matches_with_that_scene_on_disk)
{
    OnScopeExit clean_up;

    mve::Scene::Ptr scene_with_empty_bundle = [&clean_up](){
        std::string path_to_scene =
                create_scene_on_disk(0, make_bundle(0), clean_up);
        return mve::Scene::create(path_to_scene);
    }();
    EXPECT_TRUE(bundle_cameras_match(
            load_bundle_directly_from(scene_with_empty_bundle->get_path()),
            scene_with_empty_bundle->get_bundle()));

    mve::Scene::Ptr scene_with_non_empty_bundle = [&clean_up](){
        std::string path_to_scene =
                create_scene_on_disk(3, make_bundle(23), clean_up);
        return mve::Scene::create(path_to_scene);
    }();
    EXPECT_TRUE(bundle_cameras_match(
            load_bundle_directly_from(scene_with_non_empty_bundle->get_path()),
            scene_with_non_empty_bundle->get_bundle()));
}

//== Creating a scene with missing files or directories ========================

TEST(SceneTest,
     create_scene_throws_an_exception_if_the_directory_does_not_exist)
{
    std::string not_a_directory = tmpnam(nullptr);
    EXPECT_THROW(mve::Scene::create(not_a_directory), util::Exception);
}

TEST(SceneTest,
     create_scene_throws_an_exception_if_the_views_subdirectory_does_not_exist)
{
    OnScopeExit clean_up;

    std::string directory_with_no_views_subdir = [&clean_up]() {
        std::string directory = tmpnam(nullptr);
        util::fs::mkdir(directory.c_str());
        clean_up.unlink(directory);
        std::string bundle_file = util::fs::join_path(directory, "synth_0.out");
        mve::save_mve_bundle(make_bundle(0), bundle_file);
        return directory;
    }();
    EXPECT_THROW(mve::Scene::create(directory_with_no_views_subdir),
                 util::Exception);
}

TEST(SceneTest,
     creating_a_scene_on_a_directory_with_no_bundle_file_makes_get_bundle_throw)
{
    OnScopeExit clean_up;

    std::string directory_missing_bundle_file
            = create_scene_on_disk(0, nullptr, clean_up);
    mve::Scene::Ptr scene_missing_bundle
            = mve::Scene::create(directory_missing_bundle_file);
    EXPECT_THROW(scene_missing_bundle->get_bundle(),
                 util::Exception);
}

//== Test loading into an existing scene =======================================

TEST(SceneTest,
     when_load_is_called_on_a_scene_its_path_updates_accordingly)
{
    OnScopeExit clean_up;

    std::string directory_to_load
            = create_scene_on_disk(0, make_bundle(3), clean_up);
    mve::Scene::Ptr scene = [&clean_up](){
        std::string scene_path =
                create_scene_on_disk(13, make_bundle(3), clean_up);
        return mve::Scene::create(scene_path);
    }();
    scene->load_scene(directory_to_load);
    EXPECT_EQ(directory_to_load, scene->get_path());
}

TEST(SceneTest,
     when_load_is_called_on_a_scene_its_views_update_accordingly)
{
    using Views = mve::Scene::ViewList;
    OnScopeExit clean_up;

    mve::Scene::Ptr scene = [&clean_up](){
        std::string scene_path =
                create_scene_on_disk(13, make_bundle(3), clean_up);
        return mve::Scene::create(scene_path);
    }();
    std::string loaded_path =
            create_scene_on_disk(9, make_bundle(4), clean_up);
    scene->load_scene(loaded_path);
    Views views_from_disk = load_views_directly_from(loaded_path);
    EXPECT_TRUE(views_match(views_from_disk, scene->get_views()));
}

TEST(SceneTest,
     when_load_is_called_on_a_scene_its_bundle_updates_accordingly)
{
    OnScopeExit clean_up;

    mve::Scene::Ptr scene = [&clean_up](){
        std::string scene_path =
                create_scene_on_disk(13, make_bundle(0), clean_up);
        return mve::Scene::create(scene_path);
    }();
    std::string loaded_path = create_scene_on_disk(0, make_bundle(5), clean_up);
    scene->load_scene(loaded_path);
    EXPECT_TRUE(bundle_cameras_match(load_bundle_directly_from(loaded_path),
                                     scene->get_bundle()));
}

//== Loading a scene with missing files or directories =========================

TEST(SceneTest,
     load_throws_an_exception_if_the_directory_does_not_exist)
{
    OnScopeExit clean_up;

    std::string not_a_directory = tmpnam(nullptr);
    mve::Scene::Ptr scene = [&clean_up](){
        std::string path_to_scene =
                create_scene_on_disk(0, make_bundle(0), clean_up);
        return mve::Scene::create(path_to_scene);
    }();
    EXPECT_THROW(scene->load_scene(not_a_directory), util::Exception);
}

TEST(SceneTest,
     load_throws_an_exception_if_the_views_subdirectory_does_not_exist)
{
    OnScopeExit clean_up;

    std::string directory_with_no_views_subdir = [&clean_up]() {
        std::string directory = tmpnam(nullptr);
        util::fs::mkdir(directory.c_str());
        clean_up.unlink(directory);
        std::string bundle_file = util::fs::join_path(directory, "synth_0.out");
        mve::save_mve_bundle(make_bundle(0), bundle_file);
        return directory;
    }();
    std::string not_a_directory = tmpnam(nullptr);
    mve::Scene::Ptr scene = [&clean_up](){
        std::string path_to_scene =
                create_scene_on_disk(0, make_bundle(0), clean_up);
        return mve::Scene::create(path_to_scene);
    }();
    EXPECT_THROW(scene->load_scene(directory_with_no_views_subdir),
                 util::Exception);
}

TEST(SceneTest,
     loading_from_a_directory_with_no_bundle_file_makes_get_bundle_throw)
{
    OnScopeExit clean_up;

    std::string directory_missing_bundle_file
            = create_scene_on_disk(0, nullptr, clean_up);
    mve::Scene::Ptr scene = [&clean_up](){
        std::string path_to_scene =
                create_scene_on_disk(0, make_bundle(0), clean_up);
        return mve::Scene::create(path_to_scene);
    }();
    scene->load_scene(directory_missing_bundle_file);
    EXPECT_THROW(scene->get_bundle(), util::Exception);
}

//== Test saving onto disk =====================================================

TEST(SceneTest,
     when_save_is_called_on_a_scene_the_scene_on_disk_updates_accordingly)
{
    using ViewList = mve::Scene::ViewList;
    OnScopeExit clean_up;

    mve::Scene::Ptr dirty_scene = [&clean_up](){
        std::string scene_path =
                create_scene_on_disk(13, nullptr, clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(scene_path);
        make_a_clean_view_dirty(scene);
        scene->set_bundle(make_bundle(3));
        return scene;
    }();

    dirty_scene->save_scene();

    EXPECT_TRUE(bundle_cameras_match(
            load_bundle_directly_from(dirty_scene->get_path()),
            dirty_scene->get_bundle()));
    ViewList loaded_views = load_views_directly_from(dirty_scene->get_path());
    EXPECT_TRUE(views_match(loaded_views, dirty_scene->get_views()));
}

TEST(SceneTest,
     when_save_bundle_is_called_on_a_scene_only_the_bundle_is_updated_on_disk)
{
    using ViewList = mve::Scene::ViewList;
    OnScopeExit clean_up;

    mve::Scene::Ptr dirty_scene = [&clean_up](){
        std::string scene_path =
                create_scene_on_disk(13, nullptr, clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(scene_path);
        make_a_clean_view_dirty(scene);
        scene->set_bundle(make_bundle(3));
        return scene;
    }();

    dirty_scene->save_bundle();

    EXPECT_TRUE(bundle_cameras_match(
            load_bundle_directly_from(dirty_scene->get_path()),
            dirty_scene->get_bundle()));
    ViewList loaded_views = load_views_directly_from(dirty_scene->get_path());
    EXPECT_FALSE(views_match(loaded_views, dirty_scene->get_views()));
}

TEST(SceneTest,
     when_save_views_is_called_on_a_scene_only_the_views_are_updated_on_disk)
{
    using ViewList = mve::Scene::ViewList;
    OnScopeExit clean_up;

    mve::Scene::Ptr dirty_scene = [&clean_up](){
        std::string scene_path =
                create_scene_on_disk(13, make_bundle(0), clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(scene_path);
        make_a_clean_view_dirty(scene);
        scene->set_bundle(make_bundle(3));
        return scene;
    }();

    dirty_scene->save_views();

    EXPECT_FALSE(bundle_cameras_match(
            load_bundle_directly_from(dirty_scene->get_path()),
            dirty_scene->get_bundle()));
    ViewList loaded_views = load_views_directly_from(dirty_scene->get_path());
    EXPECT_TRUE(views_match(loaded_views, dirty_scene->get_views()));
}

//== Test resetting a scene's bundle ===========================================

TEST(SceneTest,
     reset_bundle_restores_the_bundle_to_its_state_on_disk)
{
    OnScopeExit clean_up;

    mve::Scene::Ptr scene_with_dirty_bundle = [&clean_up](){
        std::string scene_path =
                create_scene_on_disk(13, make_bundle(15), clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(scene_path);
        scene->set_bundle(make_bundle(0));
        return scene;
    }();
    scene_with_dirty_bundle->reset_bundle();
    EXPECT_TRUE(bundle_cameras_match(
            load_bundle_directly_from(scene_with_dirty_bundle->get_path()),
            scene_with_dirty_bundle->get_bundle()));
}

//== Test the dirty state of a scene ===========================================

TEST(SceneTest,
     a_clean_scene_becomes_dirty_if_any_of_its_views_become_dirty)
{
    OnScopeExit clean_up;

    mve::Scene::Ptr clean_scene = [&clean_up](){
        std::string path_to_scene =
                create_scene_on_disk(10, make_bundle(8), clean_up);
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
        std::string path_to_scene =
                create_scene_on_disk(5, mve::Bundle::create(), clean_up);
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
        std::string path_to_scene =
                create_scene_on_disk(7, make_bundle(3), clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(path_to_scene);
        make_a_clean_view_dirty(scene);
        return scene;
    }();

    dirty_scene->set_bundle(make_bundle(0));
    EXPECT_TRUE(dirty_scene->is_dirty());

    make_a_clean_view_dirty(dirty_scene);
    EXPECT_TRUE(dirty_scene->is_dirty());
}

TEST(SceneTest,
     saving_a_dirty_scene_cleans_it)
{
    OnScopeExit clean_up;

    mve::Scene::Ptr dirty_scene = [&clean_up]() {
        std::string path_to_scene =
                create_scene_on_disk(10, make_bundle(1), clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(path_to_scene);
        make_a_clean_view_dirty(scene);
        scene->set_bundle(make_bundle(0));
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
        std::string path_to_scene =
                create_scene_on_disk(4, make_bundle(4), clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(path_to_scene);
        for(mve::View::Ptr& view : scene->get_views())
            make_dirty(view);
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
        std::string path_to_scene =
                create_scene_on_disk(5, make_bundle(7), clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(path_to_scene);
        scene->set_bundle(make_bundle(6));
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
        std::string path_to_scene =
                create_scene_on_disk(10, make_bundle(3), clean_up);
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
        std::string path_to_scene =
                create_scene_on_disk(7, make_bundle(8), clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(path_to_scene);
        make_a_clean_view_dirty(scene);
        scene->set_bundle(mve::Bundle::create());
        return scene;
    }();
    scene_with_dirty_view->save_views();
    EXPECT_TRUE(scene_with_dirty_view->is_dirty());
}

TEST(SceneTest,
     reset_bundle_cleans_a_scene_if_only_its_bundle_is_dirty)
{
    OnScopeExit clean_up;

    mve::Scene::Ptr scene_with_dirty_bundle_and_clean_views = [&clean_up]() {
        std::string path_to_scene =
                create_scene_on_disk(10, make_bundle(3), clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(path_to_scene);
        scene->set_bundle(mve::Bundle::create());
        return scene;
    }();
    scene_with_dirty_bundle_and_clean_views->reset_bundle();
    EXPECT_FALSE(scene_with_dirty_bundle_and_clean_views->is_dirty());
}

TEST(SceneTest,
     reset_bundle_does_not_clean_a_scene_if_any_of_its_views_is_dirty)
{
    OnScopeExit clean_up;

    mve::Scene::Ptr scene_with_dirty_bundle_and_clean_views = [&clean_up]() {
        std::string path_to_scene =
                create_scene_on_disk(10, make_bundle(3), clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(path_to_scene);
        make_a_clean_view_dirty(scene);
        scene->set_bundle(mve::Bundle::create());
        return scene;
    }();
    scene_with_dirty_bundle_and_clean_views->reset_bundle();
    EXPECT_TRUE(scene_with_dirty_bundle_and_clean_views->is_dirty());
}

TEST(SceneTest,
     saving_the_dirty_views_of_a_scene_cleans_the_scene_if_its_bundle_is_clean)
{
    using View = mve::View::Ptr;
    OnScopeExit clean_up;

    mve::Scene::Ptr scene_with_dirty_views_and_clean_bundle = [&clean_up]() {
        std::string path_to_scene =
                create_scene_on_disk(10, make_bundle(6), clean_up);
        mve::Scene::Ptr scene = mve::Scene::create(path_to_scene);
        for(int i = 0; i < 5; ++i)
            make_a_clean_view_dirty(scene);
        return scene;
    }();

    for (View& view : scene_with_dirty_views_and_clean_bundle->get_views())
        if (view->is_dirty())
            view->save_view();

    EXPECT_FALSE(scene_with_dirty_views_and_clean_bundle->is_dirty());
}

//== End of tests ==============================================================

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

mve::Scene::ViewList
load_views_directly_from(const std::string& scene_directory)
{
    std::string view_directory = util::fs::join_path(scene_directory, "views");
    util::fs::Directory directory(view_directory);
    mve::Scene::ViewList list;
    list.reserve(directory.size());

    for (util::fs::File& file : directory)
        list.push_back(mve::View::create(file.get_absolute_name()));

    return list;
}

mve::Bundle::Ptr
load_bundle_directly_from(const std::string& scene_directory)
{
    std::string bundle_file =
            util::fs::join_path(scene_directory, "synth_0.out");
    return mve::load_mve_bundle(bundle_file);
}

mve::Bundle::Ptr
make_bundle(const std::size_t camera_count)
{
    mve::Bundle::Ptr bundle = mve::Bundle::create();
    bundle->get_cameras().reserve(camera_count);
    for(size_t i = 1; i <= camera_count; ++i)
    {
        mve::CameraInfo camera;
        camera.flen = 1.0f + 2.0f / i
                    * (i % 3 == 1); // one in every 3 cameras is not valid
        camera.trans[0] = i - 10.0f;
        camera.trans[1] = 1.0f / i;
        camera.trans[2] = 10.0f - i;
        camera.paspect = 0.5f + 1.0f / i;
        bundle->get_cameras().push_back(std::move(camera));
    }

    return bundle;
}

bool
views_match(mve::Scene::ViewList& lhs, mve::Scene::ViewList& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for(mve::View::Ptr& left_view : lhs)
    {
        mve::Scene::ViewList::iterator found = std::find_if(
                rhs.begin(),
                rhs.end(),
                [&left_view](mve::View::Ptr& right_view) {
                    return left_view->get_id() == right_view->get_id();
                }
        );
        if (found == rhs.end() || left_view->get_name() != (*found)->get_name())
            return false;
    }

    return true;
}

bool
bundle_cameras_match(mve::Bundle::Ptr lhs, mve::Bundle::ConstPtr rhs)
{
    const auto match = [](float l, float r) {
        constexpr float epsilon = 1e-5f;
        return (r == 0.0f)
               ? l < epsilon
               : (std::abs((l / r) - 1.0f) < epsilon);
    };

    const auto match_n = [&match](const float l[],
                                  const float r[],
                                  std::size_t count) {
        for (std::size_t i = 0; i < count; ++i)
            if (!match(l[i], r[i]))
                return false;
        return true;
    };

    if (lhs->get_cameras().size() != rhs->get_cameras().size() ||
        lhs->get_features().size() != rhs->get_features().size())
        return false;

    for(std::size_t i = 0; i < lhs->get_cameras().size(); ++i)
    {
        const mve::CameraInfo& left_cam = lhs->get_cameras()[i];
        const mve::CameraInfo& right_cam = rhs->get_cameras()[i];
        if( !match(left_cam.flen, right_cam.flen)
            || !match(left_cam.paspect, right_cam.paspect)
            || !match_n(left_cam.dist, right_cam.dist, 2)
            || !match_n(left_cam.ppoint, right_cam.ppoint, 2)
            || !match_n(left_cam.trans, right_cam.trans, 3)
            || !match_n(left_cam.rot, right_cam.rot, 9))
            return false;
    }

    return true;
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