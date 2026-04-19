import { Router } from 'express';
import { JobController } from '../controllers/JobController';

const router = Router();
const controller = new JobController();

router.post('/run', (req, res, next) => {
  void controller.runBatch(req, res, next);
});

export default router;
