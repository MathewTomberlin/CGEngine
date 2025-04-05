namespace CGEngine {
	class Global {
	public:
        static V2f getGlobalPosition(Transform transform) {
            return transform.transformPoint({ 0,0 });
        }

        static Angle getGlobalRotation(Transform transform) {
            V2f dir = getForward(transform);
            float angle = atan2(dir.y, dir.x) * 180.0f / (float)M_PI;
            if (angle < 0) angle += 360.0f;
            return degrees(angle);
        }

        static V2f getForward(Transform transform) {
            Vector2f wPos = transform.transformPoint({ 0,0 });
            Vector2f rPos = transform.transformPoint({ 1,0 });
            Vector2f dir = (rPos - wPos);
            if (dir.lengthSquared() > 0.001f) dir = dir.normalized();
            return dir;
        }

        static V2f getRight(Transform transform) {
            Vector2f wPos = transform.transformPoint({ 0,0 });
            Vector2f uPos = transform.transformPoint({ 0,1 });
            Vector2f dir = (uPos - wPos);
            if (dir.lengthSquared() > 0.001f) dir = dir.normalized();
            return dir;
        }

        static V2f getGlobalScale(Transform transform) {
            Transform r_wT = transform.rotate(getInverseGlobalRotation(transform));
            Vector2f wPos = transform.transformPoint({ 0,0 });
            Vector2f rPos = transform.transformPoint({ 1,1 });
            Vector2f dir = (rPos - wPos);
            return dir;
        }
        static V2f getInverseGlobalScale(Transform transform) {
            return Vector2f({ 1.f,1.f }).componentWiseDiv(getGlobalScale(transform));
        }

        static Angle getInverseGlobalRotation(Transform transform) {
            return degrees(-getGlobalRotation(transform).asDegrees());
        }
	};
}