#include <iostream>
#include <deque>
#include <inttypes.h>
#include <math.h>

#include "asserts.hpp"
#include "color_utils.hpp"
#include "entity.hpp"
#include "foreach.hpp"
#include "frame.hpp"
#include "particle_system.hpp"
#include "preferences.hpp"
#include "string_utils.hpp"
#include "texture.hpp"
#include "wml_node.hpp"
#include "wml_utils.hpp"
#include "weather_particle_system.hpp"
#include "water_particle_system.hpp"
#include "raster.hpp"

namespace {

class particle_animation {
public:
	explicit particle_animation(wml::const_node_ptr node) :
	  id_(node->attr("id")),
	  texture_(graphics::texture::get(node->attr("image"))),
	  duration_(wml::get_int(node, "duration"))
	{
		rect base_area(node->has_attr("rect") ? rect(node->attr("rect")) :
	           rect(wml::get_int(node, "x"),
	                wml::get_int(node, "y"),
	                wml::get_int(node, "w", texture_.width()),
	                wml::get_int(node, "h", texture_.height())));
		width_ = base_area.w()*2;
		height_ = base_area.h()*2;
		int nframes = wml::get_int(node, "frames", 1);
		if(nframes < 1) {
			nframes = 1;
		}

		const int nframes_per_row = wml::get_int(node, "frames_per_row", -1);
		const int pad = wml::get_int(node, "pad");

		int row = 0, col = 0;
		for(int n = 0; n != nframes; ++n) {
			rect area(base_area.x() + col*(base_area.w()+pad),
			          base_area.y() + row*(base_area.h()+pad),
					  base_area.w(), base_area.h());

			frame_area a;
			a.u1 = GLfloat(area.x())/GLfloat(texture_.width());
			a.u2 = GLfloat(area.x2())/GLfloat(texture_.width());
			a.v1 = GLfloat(area.y())/GLfloat(texture_.height());
			a.v2 = GLfloat(area.y2())/GLfloat(texture_.height());

			frames_.push_back(a);

			++col;
			if(col == nframes_per_row) {
				col = 0;
				++row;
			}
		}
	}

	struct frame_area {
		GLfloat u1, v1, u2, v2;
	};

	const frame_area& get_frame(int t) const {
		int index = t/duration_;
		if(index < 0) {
			index = 0;
		} else if(index >= frames_.size()) {
			index = frames_.size() - 1;
		}

		return frames_[index];
	}

	void set_texture() const {
		texture_.set_as_current_texture();
	}

	int width() const { return width_; }
	int height() const { return height_; }
private:
	std::string id_;
	graphics::texture texture_;

	std::vector<frame_area> frames_;
	int duration_;
	int width_, height_;
};

struct simple_particle_system_info {
	simple_particle_system_info(wml::const_node_ptr node)
	  : spawn_rate_(wml::get_int(node, "spawn_rate", 1)),
	    spawn_rate_random_(wml::get_int(node, "spawn_rate_random")),
	    system_time_to_live_(wml::get_int(node, "system_time_to_live", -1)),
	    time_to_live_(wml::get_int(node, "time_to_live", 50)),
	    min_x_(wml::get_int(node, "min_x", 0)),
	    max_x_(wml::get_int(node, "max_x", 0)),
	    min_y_(wml::get_int(node, "min_y", 0)),
	    max_y_(wml::get_int(node, "max_y", 0)),
		velocity_x_(wml::get_int(node, "velocity_x", 0)),
		velocity_y_(wml::get_int(node, "velocity_y", 0)),
		velocity_x_rand_(wml::get_int(node, "velocity_x_random", 0)),
		velocity_y_rand_(wml::get_int(node, "velocity_y_random", 0)),
		velocity_magnitude_(wml::get_int(node, "velocity_magnitude", 0)),
		velocity_magnitude_rand_(wml::get_int(node, "velocity_magnitude_random", 0)),
		velocity_rotate_(wml::get_int(node, "velocity_rotate", 0)),
		velocity_rotate_rand_(wml::get_int(node, "velocity_rotate_random", 0)),
		accel_x_(wml::get_int(node, "accel_x", 0)),
		accel_y_(wml::get_int(node, "accel_y", 0)),
		delta_r_(wml::get_int(node, "delta_r", 0)),
		delta_g_(wml::get_int(node, "delta_g", 0)),
		delta_b_(wml::get_int(node, "delta_b", 0)),
		delta_a_(wml::get_int(node, "delta_a", 0))
	{
	}
	int spawn_rate_, spawn_rate_random_;
	int system_time_to_live_;
	int time_to_live_;
	int min_x_, max_x_, min_y_, max_y_;
	int velocity_x_, velocity_y_;
	int velocity_x_rand_, velocity_y_rand_;
	int velocity_magnitude_, velocity_magnitude_rand_;
	int velocity_rotate_, velocity_rotate_rand_;
	int accel_x_, accel_y_;

	int delta_r_, delta_g_, delta_b_, delta_a_;
};

class simple_particle_system_factory : public particle_system_factory {
public:
	explicit simple_particle_system_factory(wml::const_node_ptr node);
	~simple_particle_system_factory() {}

	particle_system_ptr create(const entity& e) const;

	std::vector<particle_animation> frames_;

	simple_particle_system_info info_;
};

simple_particle_system_factory::simple_particle_system_factory(wml::const_node_ptr node)
  : info_(node)
{
	FOREACH_WML_CHILD(frame_node, node, "animation") {
		frames_.push_back(particle_animation(frame_node));
	}
}

class simple_particle_system : public particle_system
{
public:
	simple_particle_system(const entity& e, const simple_particle_system_factory& factory);
	~simple_particle_system() {}

	bool is_destroyed() const { return info_.system_time_to_live_ == 0 || info_.spawn_rate_ == 0 && particles_.empty(); }
	void process(const level& lvl, const entity& e);
	void draw(const rect& area, const entity& e) const;

private:
	variant get_value(const std::string& key) const {
		if(key == "spawn_rate") {
			return variant(info_.spawn_rate_);
		} else {
			return variant();
		}
	}

	void set_value(const std::string& key, const variant& value) {
		if(key == "spawn_rate") {
			info_.spawn_rate_ = value.as_int();
		} else if(key == "min_x") {
			info_.min_x_ = value.as_int();
		} else if(key == "max_x") {
			info_.max_x_ = value.as_int();
		} else if(key == "min_y") {
			info_.min_y_ = value.as_int();
		} else if(key == "max_y") {
			info_.max_y_ = value.as_int();
		} else if(key == "velocity_x") {
			std::cerr << "SET VELOCITY_X PARTICLES: " << value.as_int() << "\n";
			info_.velocity_x_ = value.as_int();
		} else if(key == "velocity_y") {
			info_.velocity_y_ = value.as_int();
		}
	}

	const simple_particle_system_factory& factory_;
	simple_particle_system_info info_;

	int cycle_;

	struct particle {
		GLfloat pos[2];
		const particle_animation* anim;
		GLfloat velocity[2];
	};

	struct generation {
		int members;
		int created_at;
	};

	std::deque<particle> particles_;
	std::deque<generation> generations_;

	int spawn_buildup_;
};

simple_particle_system::simple_particle_system(const entity& e, const simple_particle_system_factory& factory)
  : factory_(factory), info_(factory.info_), cycle_(0), spawn_buildup_(0)
{
}

void simple_particle_system::process(const level& lvl, const entity& e)
{
	--info_.system_time_to_live_;
	++cycle_;

	while(!generations_.empty() && cycle_ - generations_.front().created_at == info_.time_to_live_) {
		particles_.erase(particles_.begin(), particles_.begin() + generations_.front().members);
		generations_.pop_front();
	}

	std::deque<particle>::iterator p = particles_.begin();
	foreach(generation& gen, generations_) {
		for(int n = 0; n != gen.members; ++n) {
			p->pos[0] += p->velocity[0];
			p->pos[1] += p->velocity[1];
			p->velocity[0] += info_.accel_x_/1000.0;
			p->velocity[1] += info_.accel_y_/1000.0;
			++p;
		}
	}

	int nspawn = info_.spawn_rate_;
	if(info_.spawn_rate_random_ > 0) {
		nspawn += rand()%info_.spawn_rate_random_;
	}

	nspawn += spawn_buildup_;
	spawn_buildup_ = nspawn%1000;
	nspawn /= 1000;

	//std::cerr << "SPAWNING " << nspawn << " PARTICLES\n";

	generation new_gen;
	new_gen.members = nspawn;
	new_gen.created_at = cycle_;

	generations_.push_back(new_gen);

	while(nspawn-- > 0) {
		particle p;
		p.pos[0] = e.x() + info_.min_x_;
		p.pos[1] = e.y() + info_.min_y_;
		p.velocity[0] = info_.velocity_x_/1000.0;
		p.velocity[1] = info_.velocity_y_/1000.0;

		if(info_.velocity_x_rand_ > 0) {
			p.velocity[0] += (rand()%info_.velocity_x_rand_)/1000.0;
		}

		if(info_.velocity_y_rand_ > 0) {
			p.velocity[1] += (rand()%info_.velocity_y_rand_)/1000.0;
		}

		int velocity_magnitude = info_.velocity_magnitude_;
		if(info_.velocity_magnitude_rand_ > 0) {
			velocity_magnitude += rand()%info_.velocity_magnitude_rand_;
		}

		if(velocity_magnitude) {
			int rotate_velocity = info_.velocity_rotate_;
			if(info_.velocity_rotate_rand_) {
				rotate_velocity += rand()%info_.velocity_rotate_rand_;
			}

			const GLfloat rotate_radians = (GLfloat(rotate_velocity)/360.0)*3.14*2.0;
			const GLfloat magnitude = velocity_magnitude/1000.0;
			p.velocity[0] += sin(rotate_radians)*magnitude;
			p.velocity[1] += cos(rotate_radians)*magnitude;
		}

		ASSERT_GT(factory_.frames_.size(), 0);
		p.anim = &factory_.frames_[rand()%factory_.frames_.size()];

		const int diff_x = info_.max_x_ - info_.min_x_;
		if(diff_x > 0) {
			p.pos[0] += (rand()%(diff_x*1000))/1000.0;
		}

		const int diff_y = info_.max_y_ - info_.min_y_;
		if(diff_y > 0) {
			p.pos[1] += (rand()%(diff_y*1000))/1000.0;
		}

		particles_.push_back(p);
	}
}

void simple_particle_system::draw(const rect& area, const entity& e) const
{
	if(particles_.empty()) {
		return;
	}

	std::deque<particle>::const_iterator p = particles_.begin();

	//all particles must have the same texture, so just set it once.
	p->anim->set_texture();
	std::vector<GLfloat>& varray = graphics::global_vertex_array();
	std::vector<GLfloat>& tcarray = graphics::global_texcoords_array();
	std::vector<GLbyte>& carray = graphics::global_vertex_color_array();

	carray.clear();
	varray.clear();
	tcarray.clear();
	foreach(const generation& gen, generations_) {
		for(int n = 0; n != gen.members; ++n) {
			const particle_animation* anim = p->anim;
			const particle_animation::frame_area& f = anim->get_frame(cycle_ - gen.created_at);

			if(info_.delta_a_){
				//Spare the bandwidth if we're opaque
				const int alpha_level = std::max(256 - info_.delta_a_*(cycle_ - gen.created_at), 0);
				const int red = 255;
				const int green = 255;
				const int blue = 255;
				for( int i = 0; i < 6; ++i){
					carray.push_back(red); carray.push_back(green); carray.push_back(blue); carray.push_back(alpha_level);
				}

			}
			//draw the first point twice, to allow drawing all particles
			//in one drawing operation.
			
			tcarray.push_back(graphics::texture::get_coord_x(f.u1));
			tcarray.push_back(graphics::texture::get_coord_y(f.v1));
			varray.push_back(p->pos[0]);
			varray.push_back(p->pos[1]);
			tcarray.push_back(graphics::texture::get_coord_x(f.u1));
			tcarray.push_back(graphics::texture::get_coord_y(f.v1));
			varray.push_back(p->pos[0]);
			varray.push_back(p->pos[1]);

			tcarray.push_back(graphics::texture::get_coord_x(f.u2));
			tcarray.push_back(graphics::texture::get_coord_y(f.v1));
			varray.push_back(p->pos[0] + anim->width());
			varray.push_back(p->pos[1]);
			tcarray.push_back(graphics::texture::get_coord_x(f.u1));
			tcarray.push_back(graphics::texture::get_coord_y(f.v2));
			varray.push_back(p->pos[0]);
			varray.push_back(p->pos[1] + anim->height());

			//draw the last point twice.
			tcarray.push_back(graphics::texture::get_coord_x(f.u2));
			tcarray.push_back(graphics::texture::get_coord_y(f.v2));
			varray.push_back(p->pos[0] + anim->width());
			varray.push_back(p->pos[1] + anim->height());
			tcarray.push_back(graphics::texture::get_coord_x(f.u2));
			tcarray.push_back(graphics::texture::get_coord_y(f.v2));
			varray.push_back(p->pos[0] + anim->width());
			varray.push_back(p->pos[1] + anim->height());
			++p;
		}
	}
	
	if(info_.delta_a_){
		glEnableClientState(GL_COLOR_ARRAY);
		glColorPointer(4, GL_UNSIGNED_BYTE, 0, &carray.front());
	}
	
	glVertexPointer(2, GL_FLOAT, 0, &varray.front());
	glTexCoordPointer(2, GL_FLOAT, 0, &tcarray.front());
	glDrawArrays(GL_TRIANGLE_STRIP, 0, varray.size()/2);
	
	if(info_.delta_a_){
		glDisableClientState(GL_COLOR_ARRAY);
	}

	glColor4f(1.0, 1.0, 1.0, 1.0);
}

particle_system_ptr simple_particle_system_factory::create(const entity& e) const
{
	return particle_system_ptr(new simple_particle_system(e, *this));
}

struct point_particle_info
{
	explicit point_particle_info(wml::const_node_ptr node)
	  : generation_rate_millis(wml::get_int(node, "generation_rate_millis")),
	    pos_x(wml::get_int(node, "pos_x")*1024),
	    pos_y(wml::get_int(node, "pos_y")*1024),
	    pos_x_rand(wml::get_int(node, "pos_x_rand")*1024),
	    pos_y_rand(wml::get_int(node, "pos_y_rand")*1024),
	    velocity_x(wml::get_int(node, "velocity_x")),
	    velocity_y(wml::get_int(node, "velocity_y")),
	    velocity_x_rand(wml::get_int(node, "velocity_x_rand")),
	    velocity_y_rand(wml::get_int(node, "velocity_y_rand")),
	    time_to_live(wml::get_int(node, "time_to_live")),
	    time_to_live_max(wml::get_int(node, "time_to_live_rand") + time_to_live) {
		std::vector<std::string> colors_str;
		util::split(node->attr("colors"), colors_str);
		foreach(const std::string& col, colors_str) {
			unsigned int val = strtoul(col.c_str(), NULL, 16);
			unsigned char* c = reinterpret_cast<unsigned char*>(&val);
			std::reverse(c, c+4);
			colors.push_back(val);
		}

		std::reverse(colors.begin(), colors.end());

		while(colors.size() < 2) {
			colors.push_back(0xFFFFFFFF);
		}

		ttl_divisor = time_to_live_max/(colors.size()-1);

		rgba[0] = wml::get_int(node, "red");
		rgba[1] = wml::get_int(node, "green");
		rgba[2] = wml::get_int(node, "blue");
		rgba[3] = wml::get_int(node, "alpha", 255);
		rgba_rand[0] = wml::get_int(node, "red_rand");
		rgba_rand[1] = wml::get_int(node, "green_rand");
		rgba_rand[2] = wml::get_int(node, "blue_rand");
		rgba_rand[3] = wml::get_int(node, "alpha_rand");
		rgba_delta[0] = wml::get_int(node, "red_delta");
		rgba_delta[1] = wml::get_int(node, "green_delta");
		rgba_delta[2] = wml::get_int(node, "blue_delta");
		rgba_delta[3] = wml::get_int(node, "alpha_delta");
	}

	int generation_rate_millis;
	int pos_x, pos_y, pos_x_rand, pos_y_rand;
	int velocity_x, velocity_y, velocity_x_rand, velocity_y_rand;
	int time_to_live, time_to_live_max;
	unsigned char rgba[4];
	unsigned char rgba_rand[4];
	char rgba_delta[4];

	std::vector<unsigned int> colors;
	int ttl_divisor;
};

class point_particle_system : public particle_system
{
public:
	point_particle_system(const entity& obj, const point_particle_info& info) : obj_(obj), info_(info), particle_generation_(0), generation_rate_millis_(info.generation_rate_millis) {
	}

	void process(const level& lvl, const entity& e) {
		particle_generation_ += generation_rate_millis_;

		particles_.erase(std::remove_if(particles_.begin(), particles_.end(), particle_destroyed), particles_.end());

		for(std::vector<particle>::iterator p = particles_.begin();
		    p != particles_.end(); ++p) {
			p->pos_x += p->velocity_x;
			p->pos_y += p->velocity_y;
			p->rgba[0] += info_.rgba_delta[0];
			p->rgba[1] += info_.rgba_delta[1];
			p->rgba[2] += info_.rgba_delta[2];
			p->rgba[3] += info_.rgba_delta[3];
			p->ttl--;
		}

		while(particle_generation_ >= 1000) {
			particles_.push_back(particle());
			particle& p = particles_.back();
			p.ttl = info_.time_to_live;
			ASSERT_LT(p.ttl/info_.ttl_divisor, info_.colors.size());
			if(info_.time_to_live_max != info_.time_to_live) {
				p.ttl += rand()%(info_.time_to_live_max - info_.time_to_live);
			}

			p.velocity_x = info_.velocity_x;
			p.velocity_y = info_.velocity_y;

			if(info_.velocity_x_rand) {
				p.velocity_x += rand()%info_.velocity_x_rand;
			}

			if(info_.velocity_y_rand) {
				p.velocity_y += rand()%info_.velocity_y_rand;
			}

			p.pos_x = e.x()*1024 + info_.pos_x;
			p.pos_y = e.y()*1024 + info_.pos_y;

			if(info_.pos_x_rand) {
				p.pos_x += rand()%info_.pos_x_rand;
			}
			
			if(info_.pos_y_rand) {
				p.pos_y += rand()%info_.pos_y_rand;
			}

			p.rgba[0] = info_.rgba[0];
			p.rgba[1] = info_.rgba[1];
			p.rgba[2] = info_.rgba[2];
			p.rgba[3] = info_.rgba[3];

			if(info_.rgba_rand[0]) {
				p.rgba[0] += rand()%info_.rgba_rand[0];
			}

			if(info_.rgba_rand[1]) {
				p.rgba[1] += rand()%info_.rgba_rand[1];
			}

			if(info_.rgba_rand[2]) {
				p.rgba[2] += rand()%info_.rgba_rand[2];
			}

			if(info_.rgba_rand[3]) {
				p.rgba[3] += rand()%info_.rgba_rand[3];
			}

			particle_generation_ -= 1000;
		}
	}

	void draw(const rect& area, const entity& e) const {
		if(particles_.empty()) {
			return;
		}

		static std::vector<GLshort> vertex;
		static std::vector<unsigned int> colors;
		vertex.resize(particles_.size()*2);
		colors.resize(particles_.size());

		unsigned int* c = &colors[0];
		GLshort* v = &vertex[0];
		for(std::vector<particle>::const_iterator p = particles_.begin();
		    p != particles_.end(); ++p) {
			*v++ = p->pos_x/1024;
			*v++ = p->pos_y/1024;
			*c++ = info_.colors[p->ttl/info_.ttl_divisor];
		}

		glColor4f(1.0, 1.0, 1.0, 1.0);

		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		glPointSize(4);

		glVertexPointer(2, GL_SHORT, 0, &vertex[0]);
		glColorPointer(4, GL_UNSIGNED_BYTE, 0, &colors[0]);
		glDrawArrays(GL_POINTS, 0, particles_.size());

		glDisableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);

		glColor4f(1.0, 1.0, 1.0, 1.0);
	}
private:
	const entity& obj_;
	const point_particle_info& info_;

	struct particle {
		GLshort velocity_x, velocity_y;
		int pos_x, pos_y;
		unsigned char rgba[4];
		int ttl;
	};

	static bool particle_destroyed(const particle& p) { return p.ttl <= 0; }

	int particle_generation_;
	int generation_rate_millis_;
	std::vector<particle> particles_;

	variant get_value(const std::string& key) const {
		return variant();
	}

	void set_value(const std::string& key, const variant& value) {
		if(key == "generation_rate") {
			generation_rate_millis_ = value.as_int();
		}
	}
};

class point_particle_system_factory : public particle_system_factory
{
public:
	explicit point_particle_system_factory(wml::const_node_ptr node)
	  : info_(node)
	{}

	particle_system_ptr create(const entity& e) const {
		return particle_system_ptr(new point_particle_system(e, info_));
	}

private:
	point_particle_info info_;
};

}

const_particle_system_factory_ptr particle_system_factory::create_factory(wml::const_node_ptr node)
{
	const std::string& type = node->attr("type");
	if(type == "simple") {
		return const_particle_system_factory_ptr(new simple_particle_system_factory(node));
	} else if (type == "weather") {
		return const_particle_system_factory_ptr(new weather_particle_system_factory(node));
	} else if (type == "water") {
		return const_particle_system_factory_ptr(new water_particle_system_factory(node));
	} else if(type == "point") {
		return const_particle_system_factory_ptr(new point_particle_system_factory(node));
	}

	ASSERT_LOG(false, "Unrecognized particle system type: " << node->attr("type"));
}

particle_system_factory::~particle_system_factory()
{
}

particle_system::~particle_system()
{
}
